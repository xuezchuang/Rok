#include "RokUnityModalWidget.h"
#include "RokModalNineSlice.h"
#include "RokPlayerHeadLayout.h"
#include "RokChangeNameLayout.h"
#include "RokPlayerKillTipAnimation.h"
#include "RokPowerActionTip.h"
#include "RokLeaderboardOverviewLayout.h"
#include "Components/RichTextBlock.h"
#include "Engine/DataTable.h"
#include "RokBuildingLevelDetails.h"
#include "RokBarracksUpgradeAttributes.h"
#include "RokBarracksSourceTextData.h"
#include "RokBarracksSourceTextWidget.h"
#include "RokBuildingSourcePreviewWidget.h"
#include "RokBuildingPreviewGlowWidget.h"
#include "Materials/MaterialInterface.h"

#include "Blueprint/WidgetTree.h"
#include "Containers/StringConv.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "RokCivilizationAnimation.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokCommanderMeshWidget.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryReader.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWidget.h"

namespace
{
const TCHAR* MainMenuAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu");
const TCHAR* CommonAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
const TCHAR* PlayerHeadAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_player_head.rok_hud_atlas_player_head");
const TCHAR* PlayerUiAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_player.rok_fullscreen_player");
const TCHAR* FrameAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame");
const TCHAR* SettingAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_setting.rok_modal_atlas_setting");
const TCHAR* CivilizationAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_civilization.rok_modal_atlas_civilization");
const TCHAR* CivilizationImageAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_civilization_img.rok_modal_atlas_civilization_img");
const TCHAR* CivilizationBackgroundAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_hero_background.rok_modal_atlas_hero_background");
const TCHAR* PageAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_page.rok_modal_atlas_page");
const TCHAR* Item0AtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item0.rok_modal_atlas_item0");
const TCHAR* Item1AtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item1.rok_modal_atlas_item1");
const TCHAR* Item5AtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5.rok_fullscreen_item5");
const TCHAR* BuildStarPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_build_star.rok_modal_build_star");
const TCHAR* WorkerBuildingPath = TEXT("/Game/RokPrototype/Textures/RuntimeCity/UnityGameApp_build_7_101_1.UnityGameApp_build_7_101_1");
const TCHAR* FarmBuildingPath = TEXT("/Game/RokPrototype/Textures/RuntimeCity/TestUnity_build_2_101_1.TestUnity_build_2_101_1");

const FIntPoint MainMenuAtlasSize(1024, 1024);
const FIntPoint CommonAtlasSize(2048, 1024);
const FIntPoint PlayerHeadAtlasSize(512, 2048);
const FIntPoint PlayerUiAtlasSize(512, 1024);
const FIntPoint FrameAtlasSize(1024, 2048);
const FIntPoint SettingAtlasSize(1024, 1024);
const FIntPoint CivilizationAtlasSize(2048, 256);
const FIntPoint CivilizationImageAtlasSize(2048, 1024);
const FIntPoint CivilizationBackgroundAtlasSize(1024, 512);
const FIntPoint PageAtlasSize(128, 2048);
const FIntPoint Item0AtlasSize(512, 512);
const FIntPoint Item1AtlasSize(256, 1024);
const FIntPoint Item5AtlasSize(1024, 2048);
const FIntPoint StarTextureSize(256, 256);

FIntRect SpriteRect(int32 X, int32 Y, int32 Width, int32 Height)
{
	return FIntRect(X, Y, X + Width, Y + Height);
}

const FIntRect WindowBodyRect = SpriteRect(1, 822, 524, 659);
const FIntRect WindowHeaderRect = SpriteRect(1, 329, 560, 60);
const FIntRect TypeMidWindowRect = SpriteRect(1, 1483, 898, 564);
const FIntRect CloseRect = SpriteRect(883, 589, 69, 69);
const FIntRect CommonPanelRect = SpriteRect(436, 165, 63, 55);
const FIntRect ResourceRowRect = SpriteRect(1952, 906, 74, 67);
const FIntRect AttributeFrameRect = CommonPanelRect;
const FIntRect PreviewFrameRect = SpriteRect(1003, 932, 400, 42);
const FIntRect ItemAmountStripRect = SpriteRect(974, 885, 120, 27);
const FIntRect HelpRect = SpriteRect(782, 237, 37, 37);
const FIntRect HelpAltRect = SpriteRect(743, 237, 37, 37);
const FIntRect ActionBackgroundRect = SpriteRect(401, 112, 20, 23);
const FIntRect ActionFillRect = SpriteRect(384, 59, 12, 19);
const FIntRect ActionAddRect = SpriteRect(1417, 422, 50, 50);
const FIntRect ActionIconRect = SpriteRect(1154, 389, 40, 40);
const FIntRect LevelArrowRect = SpriteRect(1229, 394, 24, 26);
const FIntRect FoodCurrencyRect = SpriteRect(865, 349, 80, 80);
const FIntRect WoodCurrencyRect = SpriteRect(1085, 489, 80, 80);
const FIntRect StoneCurrencyRect = SpriteRect(947, 431, 80, 80);
const FIntRect GoldCurrencyRect = SpriteRect(865, 431, 80, 80);
const FIntRect GemCurrencyRect = SpriteRect(783, 435, 80, 80);
const FIntRect ModifyButtonRect = SpriteRect(1211, 474, 138, 54);
const FIntRect ResourceBlueButtonRect = SpriteRect(1098, 571, 202, 81);
const FIntRect ResourceGoldButtonRect = SpriteRect(677, 600, 202, 81);
const FIntRect InstantUpgradeButtonRect = SpriteRect(1766, 586, 274, 84);
const FIntRect TimedUpgradeButtonRect = SpriteRect(1490, 617, 274, 84);

const FIntRect ProfileFrameRect = SpriteRect(307, 654, 96, 314);
const FIntRect ProfileFlagRect = SpriteRect(1, 597, 304, 371);
const FIntRect PictureButtonRect = SpriteRect(1, 1, 38, 38);
const FIntRect RenameRect = SpriteRect(385, 154, 32, 32);
const FIntRect RenamePermitItemRect = SpriteRect(129, 129, 126, 126);
const FIntRect ChangeRect = SpriteRect(513, 396, 32, 32);
const FIntRect ArmyRect = SpriteRect(1, 341, 126, 126);
const FIntRect MoreInfoRect = SpriteRect(1, 213, 126, 126);
const FIntRect RankingRect = SpriteRect(129, 469, 126, 126);
const FIntRect SettingsRect = SpriteRect(129, 341, 126, 126);
const FIntRect PortraitRect = SpriteRect(1, 345, 211, 211);
const FIntRect PortraitFrameRect = SpriteRect(214, 984, 211, 211);
const FIntRect CivilizationArabiaRect = SpriteRect(1659, 153, 53, 51);
const FIntRect CivilizationSelectRect = SpriteRect(704, 51, 208, 204);
const FIntRect CivilizationFlagRects[] = {
	SpriteRect(1086, 115, 170, 140),
	SpriteRect(914, 115, 170, 140),
	SpriteRect(345, 38, 170, 140),
	SpriteRect(517, 38, 170, 140),
	SpriteRect(173, 38, 170, 140),
	SpriteRect(1, 38, 170, 140),
	SpriteRect(1258, 115, 170, 140),
};
const FIntRect CivilizationMarkRects[] = {
	SpriteRect(1659, 153, 53, 51),
	SpriteRect(1668, 206, 56, 49),
	SpriteRect(914, 63, 57, 50),
	SpriteRect(1602, 149, 55, 56),
	SpriteRect(1778, 206, 44, 49),
	SpriteRect(1602, 207, 64, 48),
	SpriteRect(973, 65, 48, 48),
};
const int32 CivilizationIds[] = {101, 201, 301, 401, 501, 601, 801};
const FIntRect CivilizationSceneRects[] = {
	SpriteRect(769, 727, 766, 296),
	SpriteRect(1, 713, 766, 310),
	SpriteRect(1, 418, 766, 293),
};
const FIntRect CivilizationSoldierRects[] = {
	SpriteRect(1, 242, 206, 174),
	SpriteRect(1, 77, 168, 163),
	SpriteRect(209, 282, 163, 134),
};
const FIntRect CivilizationBackgroundRects[] = {
	SpriteRect(259, 367, 256, 144),
	SpriteRect(1, 75, 256, 144),
	SpriteRect(1, 221, 256, 144),
	SpriteRect(1, 367, 256, 144),
};



const FIntRect TabSelectedRect = SpriteRect(205, 370, 145, 103);
const FIntRect TabDarkRect = SpriteRect(205, 266, 121, 102);
const FIntRect TabIconRects[4][2] = {
	{SpriteRect(63, 871, 60, 60), SpriteRect(63, 933, 60, 60)},
	{SpriteRect(63, 995, 60, 60), SpriteRect(1, 3, 60, 60)},
	{SpriteRect(1, 65, 60, 60), SpriteRect(1, 127, 60, 60)},
	{SpriteRect(1, 189, 60, 60), SpriteRect(1, 251, 60, 60)},
};
const FIntRect ItemQualityRects[3] = {
	SpriteRect(137, 219, 134, 134),
	SpriteRect(295, 377, 134, 134),
	SpriteRect(159, 377, 134, 134),
};
const FIntRect ItemIconRects[4] = {
	SpriteRect(129, 769, 126, 126),
	SpriteRect(129, 641, 126, 126),
	SpriteRect(129, 513, 126, 126),
	SpriteRect(1, 385, 126, 126),
};

const FMargin WindowBodyMargin(493.0f / 524.0f, 278.0f / 659.0f, 15.0f / 524.0f, 291.0f / 659.0f);
const FMargin WindowHeaderMargin(539.0f / 560.0f, 0.0f, 8.0f / 560.0f, 0.0f);
const FMargin CommonPanelMargin(16.0f / 63.0f, 13.0f / 55.0f, 15.0f / 63.0f, 13.0f / 55.0f);
const FMargin ResourceRowMargin(23.0f / 74.0f, 13.0f / 67.0f, 21.0f / 74.0f, 22.0f / 67.0f);
const FMargin PreviewFrameMargin(182.0f / 400.0f, 13.0f / 42.0f, 115.0f / 400.0f, 12.0f / 42.0f);
const FMargin ProfileFrameMargin(31.0f / 96.0f, 60.0f / 314.0f, 36.0f / 96.0f, 52.0f / 314.0f);
const FMargin ActionBackgroundMargin(10.0f / 20.0f, 7.0f / 23.0f, 10.0f / 20.0f, 6.0f / 23.0f);
const FMargin ActionFillMargin(5.0f / 12.0f, 0.0f, 3.0f / 12.0f, 0.0f);
const FMargin ModifyButtonMargin(70.0f / 138.0f, 0.0f, 64.0f / 138.0f, 0.0f);

constexpr float UnityReferenceWidth = 1440.0f;
constexpr float UnityReferenceHeight = 810.0f;
constexpr int32 ChangeNameMinLength = 3;
constexpr int32 ChangeNameMaxLength = 15;
constexpr int32 RenamePermitItemId = 502010001;
constexpr int32 ChangeNameGemCost = 500;
const TCHAR* ChangeNameBlockedWordsRelativePath = TEXT("ref/client-unity/Assets/StreamingAssets/Config/Bin/Block.bin");

FText FormatNumber(int32 Value)
{
	FNumberFormattingOptions Options;
	Options.UseGrouping = true;
	return FText::AsNumber(Value, &Options);
}

int32 FindCivilizationVisualIndex(int32 CivilizationId)
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CivilizationIds); ++Index)
	{
		if (CivilizationIds[Index] == CivilizationId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 FindCivilizationUnlockedPreviewIndex(int32 CivilizationId)
{
	switch (CivilizationId)
	{
	case 101:
		return 0;
	case 201:
		return 1;
	case 501:
		return 2;
	default:
		return INDEX_NONE;
	}
}

int32 FindCivilizationBackgroundIndex(int32 CivilizationId)
{
	switch (CivilizationId)
	{
	case 101:
	case 301:
	case 401:
		return 0;
	case 201:
		return 1;
	case 501:
		return 2;
	case 601:
		return 3;
	default:
		return INDEX_NONE;
	}
}

FString FormatCivilizationBonusDescription(
	const FRokUnityCatalogCivilization& Civilization)
{
	FString Result = Civilization.BonusDescriptionZhCn;
	Result.ReplaceInline(TEXT("<color=yellow>"), TEXT(""));
	Result.ReplaceInline(TEXT("</color>"), TEXT(""));
	Result.ReplaceInline(TEXT("<b>"), TEXT(""));
	Result.ReplaceInline(TEXT("</b>"), TEXT(""));
	for (int32 Index = 0; Index < Civilization.BonusValues.Num(); ++Index)
	{
		Result.ReplaceInline(
			*FString::Printf(TEXT("{%d}"), Index),
			*FString::FromInt(Civilization.BonusValues[Index]));
	}
	return Result;
}

TArray<FRokUnityCatalogCivilization> LoadCivilizationRowsForUi(
	UGameInstance* GameInstance)
{
	if (GameInstance)
	{
		if (const URokUnityGameplayCatalogSubsystem* Catalog =
				GameInstance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
			Catalog && Catalog->IsCatalogLoaded())
		{
			return Catalog->GetCivilizations();
		}
	}

	// Isolated widget automation has no GameInstance. Load the same generated
	// catalog through the subsystem parser so tests and designer previews do
	// not introduce a second hard-coded civilization list.
	UGameInstance* PreviewGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	URokUnityGameplayCatalogSubsystem* PreviewCatalog = PreviewGameInstance
		? NewObject<URokUnityGameplayCatalogSubsystem>(PreviewGameInstance)
		: nullptr;
	return PreviewCatalog && PreviewCatalog->LoadCatalog()
		? PreviewCatalog->GetCivilizations()
		: TArray<FRokUnityCatalogCivilization>();
}
}

ERokChangeNameValidationResult URokUnityModalWidget::ValidateChangeNameCandidate(
	const FString& Candidate,
	const FString& CurrentName,
	bool bHasCurrentName,
	const TArray<FString>& BannedWords,
	bool bBannedWordsLoaded)
{
	if (Candidate.IsEmpty())
	{
		return ERokChangeNameValidationResult::Empty;
	}
	if (Candidate.Len() < ChangeNameMinLength)
	{
		return ERokChangeNameValidationResult::TooShort;
	}
	if (Candidate.Len() > ChangeNameMaxLength)
	{
		return ERokChangeNameValidationResult::TooLong;
	}

	int32 DigitStart = 0;
	if (Candidate[0] == TEXT('+') || Candidate[0] == TEXT('-'))
	{
		DigitStart = 1;
	}
	bool bOnlyDigits = DigitStart < Candidate.Len();
	for (int32 Index = DigitStart; Index < Candidate.Len() && bOnlyDigits; ++Index)
	{
		bOnlyDigits = FChar::IsDigit(Candidate[Index]);
	}
	if (bOnlyDigits)
	{
		return ERokChangeNameValidationResult::OnlyDigits;
	}
	if (bHasCurrentName && Candidate == CurrentName)
	{
		return ERokChangeNameValidationResult::SameAsCurrent;
	}
	if (!bBannedWordsLoaded)
	{
		return ERokChangeNameValidationResult::BannedWordDataUnavailable;
	}
	for (const FString& BannedWord : BannedWords)
	{
		if (!BannedWord.IsEmpty()
			&& Candidate.Contains(BannedWord, ESearchCase::IgnoreCase))
		{
			return ERokChangeNameValidationResult::BannedWord;
		}
	}
	return ERokChangeNameValidationResult::Valid;
}

#if !UE_BUILD_SHIPPING
AActor* URokUnityModalWidget::GetSelectedBuildingActorForAudit() const
{
	return SelectedBuildingActor.Get();
}
#endif

void URokUnityModalWidget::OpenPlayerProfile()
{
	ActiveMode = ERokUnityModalMode::PlayerProfile;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenPlayerHeadSelection()
{
	ActiveMode = ERokUnityModalMode::PlayerHeadSelection;
	SelectedPlayerHeadPage = 0;
	SelectedPlayerHeadIndex = INDEX_NONE;
	PlayerHeadScrollOffset = 0.0f;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok player-head UI opened: page=head catalog=12 ownership=server_unsynced submit=unavailable."));
}

void URokUnityModalWidget::OpenPlayerPowerDetails()
{
	ActiveMode = ERokUnityModalMode::PlayerPowerDetails;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok player-power UI opened: local_preview=partial authoritative_stats=server_unsynced."));
}

void URokUnityModalWidget::OpenPlayerRanking()
{
	ActiveMode = ERokUnityModalMode::PlayerRanking;
	SelectedRankingCategory = INDEX_NONE;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok player-ranking UI opened: categories=%d config_preview=1 rank_state=server_unsynced."), PlayerRankingRows.Num());
}

void URokUnityModalWidget::OpenCivilizationSelection()
{
	SelectedCivilizationId = 0;
	const TArray<FRokUnityCatalogCivilization> Civilizations =
		LoadCivilizationRowsForUi(GetGameInstance());
	for (const FRokUnityCatalogCivilization& Civilization : Civilizations)
	{
		if (!Civilization.bLocked)
		{
			SelectedCivilizationId = Civilization.Id;
			break;
		}
	}
	ActiveMode = ERokUnityModalMode::CivilizationSelection;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok civilization UI opened: source=CreateCharView visible=7 selected=%d profile_state_unchanged=1 authority=server."),
		SelectedCivilizationId);
}

void URokUnityModalWidget::OpenCivilizationChangeConfirmation()
{
	if (SelectedCivilizationId <= 0)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok civilization confirmation rejected: no selectable civilization."));
		return;
	}
	ActiveMode = ERokUnityModalMode::CivilizationChangeConfirmation;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok civilization confirmation opened: civilization=%d validation=server_unsynced submit=unavailable state_unchanged=1."),
		SelectedCivilizationId);
}

void URokUnityModalWidget::OpenChangeName()
{
	ChangeNameDraft.Reset();
	ChangeNameCurrentName.Reset();
	bChangeNameHasCurrentName = false;
	bChangeNameSubmitServiceAvailable = false;

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (const URokUnityRuntimeStateSubsystem* RuntimeState =
				GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
			RuntimeState && RuntimeState->IsRuntimeStateLoaded())
		{
			const FRokUnityRuntimePlayerProfileState ProfileState =
				RuntimeState->GetPlayerProfileState();
			bChangeNameHasCurrentName = ProfileState.bHasDisplayName;
			ChangeNameCurrentName = ProfileState.DisplayName;
		}
	}

	EnsureChangeNameBannedWordsLoaded();
	ActiveMode = ERokUnityModalMode::ChangeName;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok change-name UI opened: current_name=%s blocked_words=%d permit_item=%d gem_cost=%d service=unavailable."),
		bChangeNameHasCurrentName ? TEXT("available") : TEXT("unavailable"),
		ChangeNameBannedWords.Num(),
		RenamePermitItemId,
		ChangeNameGemCost);
}

void URokUnityModalWidget::OpenResourceInventory(int32 InitialCategory, bool bUseReferenceCaptureState)
{
	ActiveMode = ERokUnityModalMode::ResourceInventory;
	SelectedResourceCategory = FMath::Clamp(InitialCategory, 0, 3);
	bReferenceResourceCaptureState = bUseReferenceCaptureState;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenBuildingInfo(AActor* BuildingActor)
{
	ResourceBuildingReferenceType = 0;
	ActiveMode = ERokUnityModalMode::BuildingInfo;
	SelectedBuildingActor = BuildingActor;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

bool URokUnityModalWidget::IsBuildingInfoOpenForActor(const AActor* Actor) const
{
	if (!IsValid(Actor) || ActiveMode != ERokUnityModalMode::BuildingInfo
		|| SelectedBuildingActor.Get() != Actor || ResourceBuildingReferenceType != 0
		|| !GetParent() || GetVisibility() != ESlateVisibility::Visible || !ModeCanvas
		|| ModeCanvas->GetVisibility() == ESlateVisibility::Collapsed) return false;
	bool bFrame = false;
	bool bAttributes = false;
	for (UWidget* Child : ModeCanvas->GetAllChildren())
	{
		if (!IsValid(Child)) continue;
		bFrame |= Child->GetFName() == TEXT("BuildingInfoPreviewFrame");
		bAttributes |= Child->GetFName() == TEXT("BuildingInfoAttributeFrame");
	}
	return bFrame && bAttributes;
}

void URokUnityModalWidget::OpenResourceBuildingInfoReferencePreview(int32 UnityBuildingTypeId)
{
	if (UnityBuildingTypeId < 2 || UnityBuildingTypeId > 5) return;
	ResourceBuildingReferenceType = UnityBuildingTypeId;
	SelectedBuildingActor.Reset();
	ActiveMode = ERokUnityModalMode::BuildingInfo;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenBuildingUpgrade(AActor* BuildingActor)
{
	ActiveMode = ERokUnityModalMode::BuildingUpgrade;
	SelectedBuildingActor = BuildingActor;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenWarWarning(bool bUseReferenceCaptureState)
{
	ActiveMode = ERokUnityModalMode::WarWarning;
	bReferenceWarWarningCaptureState = bUseReferenceCaptureState;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenBuffList(bool bUseReferenceCaptureState)
{
	ActiveMode = ERokUnityModalMode::BuffList;
	bReferenceBuffListCaptureState = bUseReferenceCaptureState;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::SetFeatureScreenWidget(
	URokUnityFeatureScreenWidget* InFeatureScreenWidget)
{
	FeatureScreenWidget = InFeatureScreenWidget;
}

void URokUnityModalWidget::CloseModal()
{
	ClosePlayerKillTip();
	ClosePlayerPowerActionTip();
	WindowAnimation.Reset();
	BuffListAnimationRoot = nullptr;
	BuildingDescriptionRoot = nullptr;
	BuildingLevelDetailsRoot = nullptr;
	BuildingLevelDetailsScroll = nullptr;
	BuildingLevelBackButton = nullptr;
	bBuildingLevelPage = false;
	BuildingPageTime = 0.25f;
	BarracksLevelDetailsRoot = nullptr;
	BarracksLevelDetailsScroll = nullptr;
	BarracksLevelBackButton = nullptr;
	bBarracksLevelPage = false;
	BarracksLevelSelectedLevel = 0;
	BarracksLevelPageTime = 0.25f;
	CivilizationHero = nullptr;
	CivilizationHeroId = INDEX_NONE;
	bCivilizationSwitchAnimation = false;
	CivilizationDetailRoot = nullptr;
	CivilizationSelectionPulse = nullptr;
	AnimatedMode = ERokUnityModalMode::None;
	CloseReturnMode = ERokUnityModalMode::None;
	ActiveMode = ERokUnityModalMode::None;
	SelectedBuildingActor.Reset();
	bReferenceWarWarningCaptureState = false;
	bReferenceBuffListCaptureState = false;
	if (ModeCanvas)
	{
		URokUiAnimatedButton::ResetAnimationsInHierarchy(ModeCanvas);
		ModeCanvas->ClearChildren();
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void URokUnityModalWidget::RequestCloseModal()
{
	if (!IsModalOpen() || WindowAnimation.IsClosing())
	{
		return;
	}
	if (ActiveMode == ERokUnityModalMode::PlayerRanking && SelectedRankingCategory != INDEX_NONE)
	{
		// UI_Win_RankingMediator.onClose: first close returns to total panel,
		// without playing the window exit; only the next close leaves ranking.
		SelectedRankingCategory = INDEX_NONE;
		RebuildModeContent();
		return;
	}
	if (ActiveMode == ERokUnityModalMode::BuffList)
	{
		// UIManager.CloseAnimatorUI only looks for a root Animator. BuffList
		// has one on pl_pos only, so CloseUI removes it immediately; the shared
		// controller's Off clip is not invoked by this screen's dismiss path.
		CloseModal();
		return;
	}
	CloseReturnMode = ERokUnityModalMode::None;
	if (ActiveMode == ERokUnityModalMode::ChangeName
		|| ActiveMode == ERokUnityModalMode::PlayerHeadSelection
		|| ActiveMode == ERokUnityModalMode::PlayerPowerDetails
		|| ActiveMode == ERokUnityModalMode::PlayerRanking
		|| ActiveMode == ERokUnityModalMode::CivilizationSelection)
	{
		CloseReturnMode = ERokUnityModalMode::PlayerProfile;
	}
	else if (ActiveMode == ERokUnityModalMode::CivilizationChangeConfirmation)
	{
		CloseReturnMode = ERokUnityModalMode::CivilizationSelection;
	}
	WindowAnimation.Close();
	ApplyWindowAnimation();
	// Source screens without an identified shared animation stay immediate.
	AdvanceWindowAnimation(0.0f);
}

TSharedRef<SWidget> URokUnityModalWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void URokUnityModalWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(IsModalOpen() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void URokUnityModalWidget::NativeDestruct()
{
	CloseModal();
	Super::NativeDestruct();
}

void URokUnityModalWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateExpandedDesignResolution(MyGeometry.GetLocalSize());
	AdvanceWindowAnimation(InDeltaTime);
}

void URokUnityModalWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RokUnityModalRoot"));
	WidgetTree->RootWidget = RootCanvas;

	UButton* DismissButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ModalDismissMask"));
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle MaskStyle;
	MaskStyle.SetNormal(NoBrush);
	MaskStyle.SetHovered(NoBrush);
	MaskStyle.SetPressed(NoBrush);
	MaskStyle.SetDisabled(NoBrush);
	MaskStyle.SetNormalPadding(FMargin(0.0f));
	MaskStyle.SetPressedPadding(FMargin(0.0f));
	DismissButton->SetStyle(MaskStyle);
	UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ModalDimmer"));
	Dimmer->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
	Dimmer->SetVisibility(ESlateVisibility::HitTestInvisible);
	DismissButton->SetContent(Dimmer);
	DismissButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
	UCanvasPanelSlot* DismissSlot = RootCanvas->AddChildToCanvas(DismissButton);
	DismissSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	DismissSlot->SetOffsets(FMargin(0.0f));

	UScaleBox* ExpandScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("ModalUnityExpandScaleBox"));
	ExpandScaleBox->SetStretch(EStretch::ScaleToFit);
	ExpandScaleBox->SetStretchDirection(EStretchDirection::Both);
	UCanvasPanelSlot* ScaleSlot = RootCanvas->AddChildToCanvas(ExpandScaleBox);
	ScaleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ScaleSlot->SetOffsets(FMargin(0.0f));
	ScaleSlot->SetZOrder(1);

	ExpandedDesignSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ModalExpandedDesignSize"));
	ExpandedDesignSizeBox->SetWidthOverride(CurrentExpandedDesignSize.X);
	ExpandedDesignSizeBox->SetHeightOverride(CurrentExpandedDesignSize.Y);
	ExpandScaleBox->SetContent(ExpandedDesignSizeBox);

	ExpandedDesignCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ModalExpandedDesignCanvas"));
	ExpandedDesignSizeBox->SetContent(ExpandedDesignCanvas);

	ModeCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ModalModeCanvas"));
	ModeCanvas->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UCanvasPanelSlot* ModeSlot = ExpandedDesignCanvas->AddChildToCanvas(ModeCanvas);
	ModeSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ModeSlot->SetOffsets(FMargin(0.0f));

	// Prevent a click on the temporarily non-hit-testable content from
	// becoming an unintended click on the dismiss mask during entry/exit.
	AnimationInputBlocker = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("ModalAnimationInputBlocker"));
	AnimationInputBlocker->SetStyle(MaskStyle);
	AnimationInputBlocker->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* AnimationBlockerSlot = RootCanvas->AddChildToCanvas(AnimationInputBlocker);
	AnimationBlockerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	AnimationBlockerSlot->SetOffsets(FMargin(0.0f));
	AnimationBlockerSlot->SetZOrder(2);
}

void URokUnityModalWidget::RebuildModeContent(bool bOpening)
{
	ClosePlayerKillTip();
	ClosePlayerPowerActionTip();
	if (!ModeCanvas)
	{
		return;
	}

	URokUiAnimatedButton::ResetAnimationsInHierarchy(ModeCanvas);
	ModeCanvas->ClearChildren();
	FeedbackText = nullptr;
	BuffListAnimationRoot = nullptr;
	BuildingDescriptionRoot = nullptr;
	BuildingLevelDetailsRoot = nullptr;
	BuildingLevelDetailsScroll = nullptr;
	BuildingLevelBackButton = nullptr;
	bBuildingLevelPage = false;
	BuildingPageTime = 0.25f;
	BarracksLevelDetailsRoot = nullptr;
	BarracksLevelDetailsScroll = nullptr;
	BarracksLevelBackButton = nullptr;
	bBarracksLevelPage = false;
	BarracksLevelSelectedLevel = 0;
	BarracksLevelPageTime = 0.25f;
	ResetChangeNameWidgetPointers();

	switch (ActiveMode)
	{
	case ERokUnityModalMode::PlayerProfile:
		BuildType1Window(NSLOCTEXT("RokModal", "PlayerProfileTitle", "城主资料"));
		BuildPlayerProfile();
		break;
	case ERokUnityModalMode::PlayerHeadSelection:
		BuildType1Window(SelectedPlayerHeadPage == 0
			? NSLOCTEXT("RokModal", "PlayerHeadTitle", "头像选择")
			: NSLOCTEXT("RokModal", "PlayerHeadFrameTitle", "头像框选择"));
		BuildPlayerHeadSelection();
		break;
	case ERokUnityModalMode::PlayerPowerDetails:
		BuildType3Window(NSLOCTEXT("RokModal", "PlayerPowerTitle", "更多信息"));
		BuildPlayerPowerDetails();
		break;
	case ERokUnityModalMode::PlayerRanking:
		BuildType3Window(NSLOCTEXT("RokModal", "PlayerRankingTitle", "排行榜"));
		BuildPlayerRanking();
		break;
	case ERokUnityModalMode::CivilizationSelection:
		BuildCivilizationSelection();
		break;
	case ERokUnityModalMode::CivilizationChangeConfirmation:
		BuildCivilizationChangeWindow(NSLOCTEXT("RokModal", "CivilizationChangeTitle", "更换文明"));
		BuildCivilizationChangeConfirmation();
		break;
	case ERokUnityModalMode::ResourceInventory:
		BuildType2Window(NSLOCTEXT("RokModal", "ResourceTitle", "资源"));
		BuildResourceInventory();
		break;
	case ERokUnityModalMode::BuildingInfo:
	{
		FText Title = NSLOCTEXT("RokModal", "BuilderHutTitle", "工人小屋");
		if (SelectedBuildingActor.IsValid())
		{
			if (URokBuildingUpgradeSubsystem* Subsystem = GetUpgradeSubsystem())
			{
				Title = Subsystem->GetBuildingStateForActor(SelectedBuildingActor.Get()).DisplayName;
			}
		}
		if (ResourceBuildingReferenceType >= 2 && ResourceBuildingReferenceType <= 5)
		{
			TArray<FString> Headers; TArray<FRokBuildingLevelDetailsRow> Rows; FString Error, Name;
			if (RokBuildingLevelDetails::Load(ResourceBuildingReferenceType, Headers, Rows, Error, &Name)) Title = FText::FromString(Name);
			else Title = NSLOCTEXT("RokModal", "BuildingInfoUnknownTitle", "建筑说明");
		}
		BuildType2Window(Title);
		BuildBuildingInfo();
		break;
	}
	case ERokUnityModalMode::BuildingUpgrade:
	{
		FText BuildingName = NSLOCTEXT("RokModal", "FarmName", "农场");
		int32 TargetLevel = 2;
		bool bAtMaxLevel = false;
		bool bSourceBarracksUpgradeParent = false;
		if (SelectedBuildingActor.IsValid())
		{
			if (URokBuildingUpgradeSubsystem* Subsystem = GetUpgradeSubsystem())
			{
				const FRokBuildingState State = Subsystem->GetBuildingStateForActor(SelectedBuildingActor.Get());
				bSourceBarracksUpgradeParent = State.UnityBuildingTypeId == 9;
				BuildingName = State.DisplayName;
				const int32 MaxLevel = Subsystem->GetMaxBuildingLevelForActor(SelectedBuildingActor.Get());
				bAtMaxLevel = State.Level >= MaxLevel;
				TargetLevel = bAtMaxLevel ? State.Level : State.Level + 1;
			}
		}
		BuildType2Window(bAtMaxLevel
			? FText::Format(NSLOCTEXT("RokModal", "BuildingMaxLevelTitle", "{0}已达到最高等级"), BuildingName)
			: FText::Format(
				NSLOCTEXT("RokModal", "UpgradeBuildingTitle", "将{0}升至等级{1}"),
				BuildingName,
				FText::AsNumber(TargetLevel)), bSourceBarracksUpgradeParent);
		BuildBuildingUpgrade();
		break;
	}
	case ERokUnityModalMode::WarWarning:
		BuildType2Window(NSLOCTEXT("RokModal", "WarWarningTitle", "预警"));
		BuildWarWarning();
		break;
	case ERokUnityModalMode::BuffList:
		BuildBuffList();
		break;
	case ERokUnityModalMode::ChangeName:
		BuildTypeMidWindow(NSLOCTEXT("RokModal", "ChangeNameTitle", "更换昵称"));
		BuildChangeName();
		break;
	default:
		break;
	}
	UpdateWindowAnimationForMode(bOpening);
}

void URokUnityModalWidget::UpdateWindowAnimationForMode(bool bOpening)
{
	if (ActiveMode != ERokUnityModalMode::CivilizationSelection)
	{
		CivilizationHero = nullptr;
		CivilizationHeroId = INDEX_NONE;
	}
	if (ActiveMode == ERokUnityModalMode::CivilizationSelection && AnimatedMode != ActiveMode)
	{
		CivilizationListAnimationTime = 0;
		CivilizationSelectionAnimationTime = 0;
		bCivilizationSwitchAnimation = false;
	}
	if (AnimatedMode != ActiveMode || (bOpening && WindowAnimation.IsClosing()))
	{
		// Root Animator bindings from unity_ui_animation_manifest.json. Do not
		// apply a child-only Pop or a custom controller to the full window.
		ERokUiWindowAnimationStyle Style = ERokUiWindowAnimationStyle::None;
		switch (ActiveMode)
		{
		case ERokUnityModalMode::BuffList: // Only pl_pos carries UA_PopAnime.
			Style = ERokUiWindowAnimationStyle::Pop;
			break;
		case ERokUnityModalMode::PlayerProfile: // UI_Win_PlayerData
		case ERokUnityModalMode::PlayerHeadSelection:
		case ERokUnityModalMode::PlayerPowerDetails:
		case ERokUnityModalMode::PlayerRanking:
		case ERokUnityModalMode::ResourceInventory:
		case ERokUnityModalMode::BuildingInfo:
		case ERokUnityModalMode::BuildingUpgrade:
		case ERokUnityModalMode::ChangeName: // UIPlayerData/UI_Win_ChangeName.prefab root Animator
			Style = ERokUiWindowAnimationStyle::Type1;
			break;
		default:
			break;
		}
		WindowAnimation.Open(Style);
		AnimatedMode = ActiveMode;
		CloseReturnMode = ERokUnityModalMode::None;
	}
	ApplyWindowAnimation();
}

void URokUnityModalWidget::ApplyWindowAnimation()
{
	if (ActiveMode == ERokUnityModalMode::CivilizationSelection)
	{
		if (CivilizationSelectionPulse)
		{
			const auto Pulse = RokCivilizationAnimation::EvaluateSelection(CivilizationSelectionAnimationTime);
			CivilizationSelectionPulse->SetRenderScale(FVector2D(Pulse.Scale));
			CivilizationSelectionPulse->SetRenderTranslation(FVector2D(0, Pulse.Y));
			CivilizationSelectionPulse->SetRenderOpacity(Pulse.Alpha);
		}
		const auto Sample = RokCivilizationAnimation::Evaluate(CivilizationHeroAnimationTime, bCivilizationSwitchAnimation);
		if (CivilizationHero)
		{
			CivilizationHero->SetRenderTranslation(FVector2D(Sample.HeroX, 0));
			CivilizationHero->SetRenderOpacity(Sample.HeroAlpha);
		}
		if (CivilizationDetailRoot)
		{
			CivilizationDetailRoot->SetRenderTranslation(FVector2D(0, Sample.FlagY));
			CivilizationDetailRoot->SetRenderOpacity(Sample.FlagAlpha);
		}
	}
	if (ActiveMode == ERokUnityModalMode::CivilizationSelection && CivilizationListRoot)
	{
		// CreateCharMediator action=CiviChange: sv_list_country Y -200 -> 1,
		// alpha 0 -> 1 over 20 frames, then hold until frame 30. No root scale.
		const float T = FMath::Clamp(CivilizationListAnimationTime * 3.0f, 0.0f, 1.0f);
		const float Alpha = T * T * (3.0f - 2.0f * T);
		CivilizationListRoot->SetRenderTranslation(FVector2D(0, 201.0f * (1.0f - Alpha)));
		CivilizationListRoot->SetRenderOpacity(Alpha);
	}
	if (AnimationInputBlocker)
	{
		AnimationInputBlocker->SetVisibility(WindowAnimation.IsAnimating()
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ModeCanvas)
	{
		const FRokUiWindowAnimationSample& Sample = WindowAnimation.GetSample();
		const bool bChildPop = ActiveMode == ERokUnityModalMode::BuffList;
		ModeCanvas->SetRenderScale(FVector2D(bChildPop ? 1.0f : Sample.Scale));
		if (bChildPop && BuffListAnimationRoot)
		{
			// UA_PopAnime animates XY scale only, with no CanvasGroup alpha curve.
			BuffListAnimationRoot->SetRenderScale(FVector2D(Sample.Scale));
		}
		ModeCanvas->SetRenderOpacity(Sample.Opacity);
		ModeCanvas->SetVisibility(WindowAnimation.IsAnimating()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::SelfHitTestInvisible);
	}
}

void URokUnityModalWidget::AdvanceWindowAnimation(float DeltaSeconds)
{
	if (ActiveMode == ERokUnityModalMode::BuildingInfo)
	{
		BuildingPageTime = FMath::Min(0.25f, BuildingPageTime + FMath::Max(0.0f, DeltaSeconds));
		ApplyBuildingPageAnimation();
	}
	if (ActiveMode == ERokUnityModalMode::BuildingUpgrade && BarracksLevelDetailsRoot)
	{
		BarracksLevelPageTime = FMath::Min(0.25f, BarracksLevelPageTime + FMath::Max(0.0f, DeltaSeconds));
		ApplyBarracksLevelPageAnimation();
	}
	if (PlayerPowerActionTipRoot)
	{
		PlayerPowerActionTipTime = FMath::Min(.25f, PlayerPowerActionTipTime + FMath::Max(0.f, DeltaSeconds));
		PlayerPowerActionTipRoot->SetRenderScale(FVector2D(RokPowerActionTip::Scale(PlayerPowerActionTipTime)));
	}
	if (PlayerKillTipRoot)
	{
		PlayerKillTipTime = FMath::Min(0.25f, PlayerKillTipTime + FMath::Max(0.0f, DeltaSeconds));
		PlayerKillTipRoot->SetRenderScale(FVector2D(RokPlayerKillTipAnimation::Scale(PlayerKillTipTime)));
	}
	if (ActiveMode == ERokUnityModalMode::CivilizationSelection)
	{
		CivilizationListAnimationTime = FMath::Min(0.5f, CivilizationListAnimationTime + FMath::Max(0.0f, DeltaSeconds));
		CivilizationSelectionAnimationTime = FMath::Fmod(CivilizationSelectionAnimationTime + FMath::Max(0.0f, DeltaSeconds), RokCivilizationAnimation::SelectionDuration);
		CivilizationHeroAnimationTime = FMath::Min(RokCivilizationAnimation::ShowDuration + RokCivilizationAnimation::OutDuration,
			CivilizationHeroAnimationTime + FMath::Max(0.0f, DeltaSeconds));
	}
	const bool bFinishedClose = WindowAnimation.Advance(DeltaSeconds);
	ApplyWindowAnimation();
	if (bFinishedClose)
	{
		const ERokUnityModalMode ReturnMode = CloseReturnMode;
		CloseModal();
		if (ReturnMode != ERokUnityModalMode::None)
		{
			ChangeNameDraft.Reset();
			ActiveMode = ReturnMode;
			RebuildModeContent(true);
			SetVisibility(ESlateVisibility::Visible);
		}
	}
}

void URokUnityModalWidget::BuildType1Window(const FText& Title)
{
	UBorder* BodyLeft = MakeAtlasBorder(TEXT("Type1BodyLeft"), FrameAtlasPath, WindowBodyRect, FrameAtlasSize, FVector2D(570.0f, 698.0f), WindowBodyMargin);
	PlaceOnCanvas(ModeCanvas, BodyLeft, FVector2D(166.5f, 71.5f), FVector2D(570.0f, 698.0f));
	UBorder* BodyRight = MakeAtlasBorder(TEXT("Type1BodyRight"), FrameAtlasPath, WindowBodyRect, FrameAtlasSize, FVector2D(570.0f, 698.0f), WindowBodyMargin);
	BodyRight->SetRenderTransformPivot(FVector2D(0.5f));
	BodyRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, BodyRight, FVector2D(703.5f, 71.5f), FVector2D(570.0f, 698.0f));

	UBorder* HeaderLeft = MakeAtlasBorder(TEXT("Type1HeaderLeft"), FrameAtlasPath, WindowHeaderRect, FrameAtlasSize, FVector2D(587.0f, 60.0f), WindowHeaderMargin);
	PlaceOnCanvas(ModeCanvas, HeaderLeft, FVector2D(133.5f, 45.5f), FVector2D(587.0f, 60.0f), 2);
	UBorder* HeaderRight = MakeAtlasBorder(TEXT("Type1HeaderRight"), FrameAtlasPath, WindowHeaderRect, FrameAtlasSize, FVector2D(586.0f, 60.0f), WindowHeaderMargin);
	HeaderRight->SetRenderTransformPivot(FVector2D(0.5f));
	HeaderRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, HeaderRight, FVector2D(720.5f, 45.5f), FVector2D(586.0f, 60.0f), 2);

	UTextBlock* TitleText = MakeModalText(TEXT("Type1Title"), 30, FLinearColor(0.96f, 0.94f, 0.88f, 1.0f), true, ETextJustify::Center);
	TitleText->SetText(Title);
	TitleText->SetShadowOffset(FVector2D(0.0f, -2.0f));
	PlaceOnCanvas(ModeCanvas, TitleText, FVector2D(139.5f, 53.4f), FVector2D(1161.0f, 40.0f), 3);
	UButton* CloseButton = MakeAtlasButton(TEXT("Type1CloseButton"), CommonAtlasPath, CloseRect, CommonAtlasSize, FVector2D(69.0f));
	CloseButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
	PlaceOnCanvas(ModeCanvas, CloseButton, FVector2D(1218.5f, 40.0f), FVector2D(69.0f), 4);
}

void URokUnityModalWidget::BuildType2Window(const FText& Title, bool bSourceBarracksUpgradeParent)
{
	UBorder* BodyLeft = MakeAtlasBorder(TEXT("Type2BodyLeft"), FrameAtlasPath, WindowBodyRect, FrameAtlasSize, FVector2D(543.0f, 660.8f), WindowBodyMargin);
	PlaceOnCanvas(ModeCanvas, BodyLeft, bSourceBarracksUpgradeParent ? FVector2D(194.0f, 94.2f) : FVector2D(177.0f, 139.2f), FVector2D(543.0f, 660.8f));
	UBorder* BodyRight = MakeAtlasBorder(TEXT("Type2BodyRight"), FrameAtlasPath, WindowBodyRect, FrameAtlasSize, FVector2D(543.0f, 660.8f), WindowBodyMargin);
	BodyRight->SetRenderTransformPivot(FVector2D(0.5f));
	BodyRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, BodyRight, bSourceBarracksUpgradeParent ? FVector2D(703.0f, 94.2f) : FVector2D(720.0f, 139.2f), FVector2D(543.0f, 660.8f));

	UBorder* HeaderLeft = MakeAtlasBorder(TEXT("Type2HeaderLeft"), FrameAtlasPath, WindowHeaderRect, FrameAtlasSize, FVector2D(560.0f, 60.0f), WindowHeaderMargin);
	PlaceOnCanvas(ModeCanvas, HeaderLeft, FVector2D(160.0f, bSourceBarracksUpgradeParent ? 63.0f : 108.0f), FVector2D(560.0f, 60.0f), 2);
	UBorder* HeaderRight = MakeAtlasBorder(TEXT("Type2HeaderRight"), FrameAtlasPath, WindowHeaderRect, FrameAtlasSize, FVector2D(560.0f, 60.0f), WindowHeaderMargin);
	HeaderRight->SetRenderTransformPivot(FVector2D(0.5f));
	HeaderRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, HeaderRight, FVector2D(720.0f, bSourceBarracksUpgradeParent ? 63.0f : 108.0f), FVector2D(560.0f, 60.0f), 2);

	UTextBlock* TitleText = MakeModalText(TEXT("Type2Title"), 30, FLinearColor(0.96f, 0.94f, 0.88f, 1.0f), true, ETextJustify::Center);
	TitleText->SetText(Title);
	TitleText->SetShadowOffset(FVector2D(0.0f, -2.0f));
	PlaceOnCanvas(ModeCanvas, TitleText, FVector2D(307.65f, bSourceBarracksUpgradeParent ? 70.0f : 115.0f), FVector2D(824.7f, 40.0f), 3);
	UButton* CloseButton = MakeAtlasButton(TEXT("Type2CloseButton"), CommonAtlasPath, CloseRect, CommonAtlasSize, FVector2D(69.0f));
	CloseButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
	PlaceOnCanvas(ModeCanvas, CloseButton, FVector2D(1191.5f, bSourceBarracksUpgradeParent ? 54.5f : 99.5f), FVector2D(69.0f), 4);
}

void URokUnityModalWidget::BuildType3Window(const FText& Title)
{
	// Unity source: UI_Model_Window_Type3, 1148x754 centered in 1440x810.
	// The two body halves intentionally overlap by 20px and the title halves
	// overlap by 25px, matching the source prefab's mirrored nine-slice layout.
	UBorder* BodyLeft = MakeAtlasBorder(
		TEXT("Type3BodyLeft"),
		FrameAtlasPath,
		WindowBodyRect,
		FrameAtlasSize,
		FVector2D(564.0f, 711.5f),
		WindowBodyMargin);
	PlaceOnCanvas(
		ModeCanvas,
		BodyLeft,
		FVector2D(166.0f, 70.5f),
		FVector2D(564.0f, 711.5f));
	UBorder* BodyRight = MakeAtlasBorder(
		TEXT("Type3BodyRight"),
		FrameAtlasPath,
		WindowBodyRect,
		FrameAtlasSize,
		FVector2D(564.0f, 711.5f),
		WindowBodyMargin);
	BodyRight->SetRenderTransformPivot(FVector2D(0.5f));
	BodyRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(
		ModeCanvas,
		BodyRight,
		FVector2D(710.0f, 70.5f),
		FVector2D(564.0f, 711.5f));

	UBorder* HeaderLeft = MakeAtlasBorder(
		TEXT("Type3HeaderLeft"),
		FrameAtlasPath,
		WindowHeaderRect,
		FrameAtlasSize,
		FVector2D(600.0f, 60.0f),
		WindowHeaderMargin);
	PlaceOnCanvas(
		ModeCanvas,
		HeaderLeft,
		FVector2D(133.0f, 37.0f),
		FVector2D(600.0f, 60.0f),
		2);
	UBorder* HeaderRight = MakeAtlasBorder(
		TEXT("Type3HeaderRight"),
		FrameAtlasPath,
		WindowHeaderRect,
		FrameAtlasSize,
		FVector2D(600.0f, 60.0f),
		WindowHeaderMargin);
	HeaderRight->SetRenderTransformPivot(FVector2D(0.5f));
	HeaderRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(
		ModeCanvas,
		HeaderRight,
		FVector2D(708.0f, 37.0f),
		FVector2D(600.0f, 60.0f),
		2);

	UTextBlock* TitleText = MakeModalText(
		TEXT("Type3Title"),
		30,
		FLinearColor(0.96f, 0.94f, 0.88f, 1.0f),
		true,
		ETextJustify::Center);
	TitleText->SetText(Title);
	TitleText->SetShadowOffset(FVector2D(0.0f, -2.0f));
	PlaceOnCanvas(
		ModeCanvas,
		TitleText,
		FVector2D(277.0f, 35.0f),
		FVector2D(886.0f, 40.0f),
		3);
	UButton* CloseButton = MakeAtlasButton(
		TEXT("Type3CloseButton"),
		CommonAtlasPath,
		CloseRect,
		CommonAtlasSize,
		FVector2D(69.0f));
	CloseButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleCloseClicked);
	PlaceOnCanvas(
		ModeCanvas,
		CloseButton,
		FVector2D(1224.5f, 28.0f),
		FVector2D(69.0f),
		4);
}

void URokUnityModalWidget::BuildCivilizationChangeWindow(const FText& Title)
{
	// Unity source: UI_Win_CivilizationChange, Type2 family at
	// (160,55,1120,700) in the 1440x810 reference view.
	UBorder* BodyLeft = MakeAtlasBorder(
		TEXT("CivilizationChangeBodyLeft"), FrameAtlasPath, WindowBodyRect,
		FrameAtlasSize, FVector2D(543.0f, 660.8f), WindowBodyMargin);
	PlaceOnCanvas(ModeCanvas, BodyLeft, FVector2D(177.0f, 94.2f), FVector2D(543.0f, 660.8f));
	UBorder* BodyRight = MakeAtlasBorder(
		TEXT("CivilizationChangeBodyRight"), FrameAtlasPath, WindowBodyRect,
		FrameAtlasSize, FVector2D(543.0f, 660.8f), WindowBodyMargin);
	BodyRight->SetRenderTransformPivot(FVector2D(0.5f));
	BodyRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, BodyRight, FVector2D(720.0f, 94.2f), FVector2D(543.0f, 660.8f));

	UBorder* HeaderLeft = MakeAtlasBorder(
		TEXT("CivilizationChangeHeaderLeft"), FrameAtlasPath, WindowHeaderRect,
		FrameAtlasSize, FVector2D(560.0f, 60.0f), WindowHeaderMargin);
	PlaceOnCanvas(ModeCanvas, HeaderLeft, FVector2D(160.0f, 63.0f), FVector2D(560.0f, 60.0f), 2);
	UBorder* HeaderRight = MakeAtlasBorder(
		TEXT("CivilizationChangeHeaderRight"), FrameAtlasPath, WindowHeaderRect,
		FrameAtlasSize, FVector2D(560.0f, 60.0f), WindowHeaderMargin);
	HeaderRight->SetRenderTransformPivot(FVector2D(0.5f));
	HeaderRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, HeaderRight, FVector2D(720.0f, 63.0f), FVector2D(560.0f, 60.0f), 2);

	UTextBlock* TitleText = MakeModalText(
		TEXT("CivilizationChangeWindowTitle"), 30,
		FLinearColor(0.96f, 0.94f, 0.88f, 1.0f), true, ETextJustify::Center);
	TitleText->SetText(Title);
	TitleText->SetShadowOffset(FVector2D(0.0f, -2.0f));
	PlaceOnCanvas(ModeCanvas, TitleText, FVector2D(307.65f, 70.0f), FVector2D(824.7f, 40.0f), 3);

	UButton* CloseButton = MakeAtlasButton(
		TEXT("CivilizationChangeCloseButton"), CommonAtlasPath, CloseRect,
		CommonAtlasSize, FVector2D(69.0f));
	CloseButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
	PlaceOnCanvas(ModeCanvas, CloseButton, FVector2D(1191.5f, 54.5f), FVector2D(69.0f), 4);
}

void URokUnityModalWidget::BuildTypeMidWindow(const FText& Title)
{
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(
			TEXT("TypeMidFrame"),
			FrameAtlasPath,
			TypeMidWindowRect,
			FrameAtlasSize,
			FVector2D(898.0f, 564.0f)),
		FVector2D(271.0f, 123.0f),
		FVector2D(898.0f, 564.0f),
		1);

	UTextBlock* TitleText = MakeModalText(
		TEXT("TypeMidTitle"),
		30,
		FLinearColor(0.96f, 0.94f, 0.88f, 1.0f),
		true,
		ETextJustify::Center);
	TitleText->SetText(Title);
	TitleText->SetShadowOffset(FVector2D(0.0f, -2.0f));
	PlaceOnCanvas(
		ModeCanvas,
		TitleText,
		FVector2D(421.0f, 137.0f),
		FVector2D(598.0f, 40.0f),
		3);

	UButton* CloseButton = MakeAtlasButton(
		TEXT("TypeMidCloseButton"),
		CommonAtlasPath,
		CloseRect,
		CommonAtlasSize,
		FVector2D(69.0f));
	CloseButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleChangeNameCancelClicked);
	PlaceOnCanvas(
		ModeCanvas,
		CloseButton,
		FVector2D(1120.0f, 111.0f),
		FVector2D(69.0f),
		4);
}

void URokUnityModalWidget::BuildPlayerProfile()
{
	FRokUnityRuntimePlayerProfileState ProfileState;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (const URokUnityRuntimeStateSubsystem* RuntimeState =
				GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
			RuntimeState && RuntimeState->IsRuntimeStateLoaded())
		{
			ProfileState = RuntimeState->GetPlayerProfileState();
		}
	}

	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("ProfileInfoFrame"), SettingAtlasPath, ProfileFrameRect, SettingAtlasSize, FVector2D(1005.1f, 320.0f), ProfileFrameMargin), FVector2D(217.45f, 182.3f), FVector2D(1005.1f, 320.0f));
	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("ProfileHangingFlag"), SettingAtlasPath, ProfileFlagRect, SettingAtlasSize, FVector2D(304.0f, 371.0f)), FVector2D(269.0f, 167.8f), FVector2D(304.0f, 371.0f), 1);
	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("ProfilePortrait"), PlayerHeadAtlasPath, PortraitRect, PlayerHeadAtlasSize, FVector2D(156.24f)), FVector2D(342.88f, 233.83f), FVector2D(156.24f), 2);
	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("ProfilePortraitFrame"), PlayerHeadAtlasPath, PortraitFrameRect, PlayerHeadAtlasSize, FVector2D(183.15f)), FVector2D(329.43f, 220.38f), FVector2D(183.15f), 3);
	UButton* PictureButton = MakeAtlasButton(
		TEXT("ProfilePictureButton"),
		SettingAtlasPath,
		PictureButtonRect,
		SettingAtlasSize,
		FVector2D(38.0f));
	PictureButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleProfileAvatarClicked);
	PictureButton->SetToolTipText(NSLOCTEXT(
		"RokModal",
		"ProfilePictureTooltip",
		"查看头像资料边界"));
	PlaceOnCanvas(
		ModeCanvas,
		PictureButton,
		FVector2D(239.0f, 206.3f),
		FVector2D(38.0f),
		3);
	UButton* ModifyButton = MakeTextAtlasButton(
		TEXT("ProfileModifyButton"),
		CommonAtlasPath,
		ModifyButtonRect,
		CommonAtlasSize,
		FVector2D(135.0f, 54.0f),
		NSLOCTEXT("RokModal", "ProfileAvatarCatalog", "头像资料"),
		20,
		FLinearColor::White);
	ModifyButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleProfileAvatarClicked);
	PlaceOnCanvas(
		ModeCanvas,
		ModifyButton,
		FVector2D(353.5f, 423.7f),
		FVector2D(135.0f, 54.0f),
		4);

	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("PlayerNameBackground"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(372.11f, 39.82f), CommonPanelMargin), FVector2D(575.3f, 242.33f), FVector2D(372.11f, 39.82f));
	UTextBlock* RoleLabel = MakeModalText(TEXT("RoleLabel"), 20, FLinearColor(0.8f, 0.78f, 0.72f, 1.0f));
	RoleLabel->SetText(NSLOCTEXT("RokModal", "LocalGovernorLabel", "城主（本地）"));
	PlaceOnCanvas(ModeCanvas, RoleLabel, FVector2D(587.71f, 210.69f), FVector2D(140.0f, 26.84f), 2);
	UTextBlock* RoleId = MakeModalText(TEXT("RoleId"), 20, FLinearColor(0.96f, 0.94f, 0.88f, 1.0f));
	RoleId->SetText(ProfileState.bHasRoleId
		? FText::Format(
			NSLOCTEXT("RokModal", "RoleIdValue", "ID: {0}"),
			FText::FromString(ProfileState.RoleId))
		: NSLOCTEXT("RokModal", "RoleIdUnavailable", "ID: --"));
	PlaceOnCanvas(ModeCanvas, RoleId, FVector2D(755.13f, 210.69f), FVector2D(190.0f, 26.84f), 2);
	UTextBlock* PlayerName = MakeModalText(TEXT("ProfilePlayerName"), 26, FLinearColor::White, true);
	PlayerName->SetText(ProfileState.bHasDisplayName
		? FText::FromString(ProfileState.DisplayName)
		: NSLOCTEXT("RokModal", "PlayerNameUnavailable", "名称未接入"));
	PlaceOnCanvas(ModeCanvas, PlayerName, FVector2D(588.61f, 248.38f), FVector2D(308.22f, 29.0f), 2);
	UButton* RenameButton = MakeAtlasButton(
		TEXT("ProfileRenameButton"),
		SettingAtlasPath,
		RenameRect,
		SettingAtlasSize,
		FVector2D(32.0f));
	RenameButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleProfileRenameClicked);
	RenameButton->SetToolTipText(NSLOCTEXT(
		"RokModal",
		"ProfileRenameTooltip",
		"修改正式角色名称"));
	PlaceOnCanvas(
		ModeCanvas,
		RenameButton,
		FVector2D(905.0f, 246.0f),
		FVector2D(32.0f),
		3);

	UTextBlock* CivilLabel = MakeModalText(TEXT("CivilizationLabel"), 22, FLinearColor(0.8f, 0.78f, 0.72f, 1.0f), false, ETextJustify::Right);
	CivilLabel->SetText(NSLOCTEXT("RokModal", "CivilizationLabelText", "文明"));
	PlaceOnCanvas(ModeCanvas, CivilLabel, FVector2D(1058.35f, 210.69f), FVector2D(126.76f, 31.77f), 2);
	if (ProfileState.bHasCivilization
		&& ProfileState.CivilizationId == 101)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				TEXT("CivilizationIcon"),
				CivilizationAtlasPath,
				CivilizationArabiaRect,
				CivilizationAtlasSize,
				FVector2D(53.0f, 51.0f)),
			FVector2D(1053.0f, 243.0f),
			FVector2D(53.0f, 51.0f),
			2);
	}
	UTextBlock* CivilName = MakeModalText(TEXT("CivilizationName"), 26, FLinearColor(1.0f, 0.9f, 0.58f, 1.0f), true);
	CivilName->SetText(ProfileState.bHasCivilization
		? FText::FromString(ProfileState.CivilizationName)
		: NSLOCTEXT("RokModal", "CivilizationUnavailable", "--"));
	PlaceOnCanvas(ModeCanvas, CivilName, FVector2D(1108.41f, 247.99f), FVector2D(116.34f, 40.0f), 2);
	UButton* CivilizationButton = MakeAtlasButton(
		TEXT("ProfileCivilizationButton"),
		SettingAtlasPath,
		ChangeRect,
		SettingAtlasSize,
		FVector2D(32.0f));
	CivilizationButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleProfileCivilizationClicked);
	CivilizationButton->SetToolTipText(NSLOCTEXT(
		"RokModal",
		"ProfileCivilizationTooltip",
		"修改正式角色文明"));
	PlaceOnCanvas(
		ModeCanvas,
		CivilizationButton,
		FVector2D(1185.0f, 252.0f),
		FVector2D(32.0f),
		3);

	auto AddProfileStat = [this](const FName BaseName, const FVector2D Position, const FText& Label, const FText& Value)
	{
		UTextBlock* LabelText = MakeModalText(*FString::Printf(TEXT("%sLabel"), *BaseName.ToString()), 22, FLinearColor(0.8f, 0.78f, 0.72f, 1.0f));
		LabelText->SetText(Label);
		PlaceOnCanvas(ModeCanvas, LabelText, Position, FVector2D(180.0f, 30.0f), 2);
		UTextBlock* ValueText = MakeModalText(*FString::Printf(TEXT("%sValue"), *BaseName.ToString()), 20, FLinearColor::White);
		ValueText->SetText(Value);
		PlaceOnCanvas(ModeCanvas, ValueText, Position + FVector2D(0.0f, 28.4f), FVector2D(190.0f, 30.0f), 2);
	};
	AddProfileStat(
		TEXT("Alliance"),
		FVector2D(588.6f, 320.8f),
		NSLOCTEXT("RokModal", "Alliance", "联盟"),
		ProfileState.bHasAlliance
			? FText::FromString(ProfileState.AllianceName)
			: NSLOCTEXT("RokModal", "AllianceUnavailable", "--"));
	AddProfileStat(
		TEXT("Power"),
		FVector2D(794.26f, 320.8f),
		NSLOCTEXT("RokModal", "LocalPreviewScore", "本地评分"),
		ProfileState.LocalPreviewScore > 0
			? FormatNumber(ProfileState.LocalPreviewScore)
			: NSLOCTEXT("RokModal", "LocalPreviewScoreUnavailable", "--"));
	AddProfileStat(
		TEXT("Kills"),
		FVector2D(1054.58f, 320.8f),
		NSLOCTEXT("RokModal", "Kills", "击杀"),
		ProfileState.bHasKillCount
			? FText::AsNumber(ProfileState.KillCount)
			: NSLOCTEXT("RokModal", "KillsUnavailable", "--"));
	UButton* KillsHelpButton = MakeAtlasButton(
		TEXT("ProfileKillsHelpButton"),
		CommonAtlasPath,
		HelpAltRect,
		CommonAtlasSize,
		FVector2D(28.0f));
	KillsHelpButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleProfileKillsHelpClicked);
	PlaceOnCanvas(
		ModeCanvas,
		KillsHelpButton,
		FVector2D(1188.0f, 315.0f),
		FVector2D(28.0f),
		3);

	UTextBlock* ActionTitle = MakeModalText(TEXT("ActionPointTitle"), 22, FLinearColor(0.8f, 0.78f, 0.72f, 1.0f));
	ActionTitle->SetText(NSLOCTEXT("RokModal", "ActionPoints", "行动力"));
	PlaceOnCanvas(ModeCanvas, ActionTitle, FVector2D(609.3f, 384.73f), FVector2D(275.31f, 30.0f), 2);
	UButton* ActionHelpButton = MakeAtlasButton(
		TEXT("ProfileActionHelpButton"),
		CommonAtlasPath,
		HelpAltRect,
		CommonAtlasSize,
		FVector2D(28.0f));
	ActionHelpButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleProfileActionHelpClicked);
	PlaceOnCanvas(
		ModeCanvas,
		ActionHelpButton,
		FVector2D(577.0f, 382.0f),
		FVector2D(28.0f),
		3);
	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("ActionBackground"), CommonAtlasPath, ActionBackgroundRect, CommonAtlasSize, FVector2D(528.15f, 22.0f), ActionBackgroundMargin), FVector2D(577.39f, 425.73f), FVector2D(528.15f, 22.0f), 2);
	if (ProfileState.bHasActionPoints
		&& ProfileState.ActionPointsMax > 0)
	{
		const float FillWidth = 528.15f * FMath::Clamp(
			static_cast<float>(ProfileState.ActionPointsCurrent)
				/ static_cast<float>(ProfileState.ActionPointsMax),
			0.0f,
			1.0f);
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasBorder(
				TEXT("ActionFill"),
				CommonAtlasPath,
				ActionFillRect,
				CommonAtlasSize,
				FVector2D(FillWidth, 18.0f),
				ActionFillMargin),
			FVector2D(577.39f, 427.73f),
			FVector2D(FillWidth, 18.0f),
			3);
	}
	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("ActionIcon"), CommonAtlasPath, ActionIconRect, CommonAtlasSize, FVector2D(34.0f)), FVector2D(581.0f, 419.0f), FVector2D(34.0f), 4);
	UTextBlock* ActionValue = MakeModalText(TEXT("ActionValue"), 20, FLinearColor::White, true, ETextJustify::Center);
	ActionValue->SetText(ProfileState.bHasActionPoints
		? FText::Format(
			NSLOCTEXT("RokModal", "ActionPointValue", "{0} / {1}"),
			FText::AsNumber(ProfileState.ActionPointsCurrent),
			FText::AsNumber(ProfileState.ActionPointsMax))
		: NSLOCTEXT("RokModal", "ActionPointUnavailable", "-- / --"));
	PlaceOnCanvas(ModeCanvas, ActionValue, FVector2D(577.39f, 421.73f), FVector2D(528.15f, 30.0f), 4);
	UButton* ActionAddButton = MakeAtlasButton(
		TEXT("ActionAddButton"),
		CommonAtlasPath,
		ActionAddRect,
		CommonAtlasSize,
		FVector2D(50.0f));
	ActionAddButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleProfileActionAddClicked);
	PlaceOnCanvas(
		ModeCanvas,
		ActionAddButton,
		FVector2D(1113.55f, 411.73f),
		FVector2D(50.0f),
		4);

	UTextBlock* SourceNote = MakeModalText(
		TEXT("ProfileSourceNote"),
		15,
		FLinearColor(0.86f, 0.8f, 0.66f, 1.0f),
		false,
		ETextJustify::Center);
	SourceNote->SetAutoWrapText(true);
	SourceNote->SetText(NSLOCTEXT("RokModal", "ProfileLocalSourceNoteCompact", "本地预览 · 角色资料待同步"));
	UE_LOG(LogTemp, Display, TEXT("Rok profile local score: total=%lld hero=%lld troop=%lld world_building_upgrade=%d authority=unsynchronized"),
		static_cast<long long>(ProfileState.LocalPreviewScore), static_cast<long long>(ProfileState.LocalHeroScore),
		static_cast<long long>(ProfileState.LocalTroopPower), ProfileState.LocalBuildingUpgradeScore);
	PlaceOnCanvas(
		ModeCanvas,
		SourceNote,
		FVector2D(566.0f, 463.0f),
		FVector2D(650.0f, 54.0f),
		4);

	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("ProfileBottomPanel"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(1008.0f, 179.3f), CommonPanelMargin), FVector2D(216.5f, 540.0f), FVector2D(1008.0f, 179.3f));
	UButton* ArmyButton = MakePlayerDataButton(
		TEXT("ProfileArmyButton"),
		ArmyRect,
		NSLOCTEXT("RokModal", "Army", "部队"));
	ArmyButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleProfileArmyClicked);
	PlaceOnCanvas(
		ModeCanvas,
		ArmyButton,
		FVector2D(384.5f, 550.0f),
		FVector2D(168.0f, 138.0f),
		2);
	UButton* MoreButton = MakePlayerDataButton(
		TEXT("ProfileMoreButton"),
		MoreInfoRect,
		NSLOCTEXT("RokModal", "PowerDetails", "战力详情"));
	MoreButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleProfileMoreClicked);
	PlaceOnCanvas(
		ModeCanvas,
		MoreButton,
		FVector2D(552.5f, 550.0f),
		FVector2D(168.0f, 138.0f),
		2);
	UButton* RankingButton = MakePlayerDataButton(
		TEXT("ProfileRankingButton"),
		RankingRect,
		NSLOCTEXT("RokModal", "Ranking", "排行榜"));
	RankingButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleProfileRankingClicked);
	PlaceOnCanvas(
		ModeCanvas,
		RankingButton,
		FVector2D(720.5f, 550.0f),
		FVector2D(168.0f, 138.0f),
		2);
	UButton* SettingsButton = MakePlayerDataButton(
		TEXT("ProfileSettingsButton"),
		SettingsRect,
		NSLOCTEXT("RokModal", "Settings", "设置"));
	SettingsButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleProfileSettingsClicked);
	PlaceOnCanvas(
		ModeCanvas,
		SettingsButton,
		FVector2D(888.5f, 550.0f),
		FVector2D(168.0f, 138.0f),
		2);

	FeedbackText = MakeModalText(
		TEXT("ProfileNavigationFeedback"),
		16,
		FLinearColor(0.96f, 0.72f, 0.34f, 1.0f),
		true,
		ETextJustify::Center);
	FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(
		ModeCanvas,
		FeedbackText,
		FVector2D(375.0f, 691.0f),
		FVector2D(690.0f, 26.0f),
		4);
}

void URokUnityModalWidget::BuildPlayerHeadSelection()
{
	using namespace RokPlayerHeadLayout;
	// UI_Win_PlayerHeadPicMediator: group 1 is heads, other rows are frames.
	// get==1 is available without the server-owned list. Other ownership is unknown.
	TArray<FRokPlayerHeadRow> Catalog;
	FString CatalogError;
	PlayerHeadPageRows.Reset();
	if (!RokPlayerHeadCatalog::Load(Catalog, CatalogError))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *CatalogError);
		UTextBlock* Missing = MakeModalText(TEXT("PlayerHeadCatalogMissing"), 24, FLinearColor::White);
		Missing->SetText(NSLOCTEXT("RokModal", "PlayerHeadCatalogMissing", "头像目录不可用，请检查迁移数据。"));
		PlaceOnCanvas(ModeCanvas, Missing, FVector2D(300, 230), FVector2D(800, 60), 3);
		return;
	}
	FRokUnityRuntimePlayerProfileState Profile;
	if (const UGameInstance* Instance = GetGameInstance())
	{
		if (const auto* Runtime = Instance->GetSubsystem<URokUnityRuntimeStateSubsystem>())
			Profile = Runtime->GetPlayerProfileState();
	}
	const int32 CurrentId = SelectedPlayerHeadPage == 0 ? Profile.DefaultHeadId : Profile.DefaultFrameId;
	for (const auto& Row : Catalog)
		if (Row.Group == SelectedPlayerHeadPage + 1) PlayerHeadPageRows.Add(Row);
	if (PlayerHeadPageRows.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerHead catalog has no rows for page %d"), SelectedPlayerHeadPage);
		return;
	}
	if (SelectedPlayerHeadIndex == INDEX_NONE)
		SelectedPlayerHeadIndex = PlayerHeadPageRows.IndexOfByPredicate([CurrentId](const auto& Row) { return Row.Id == CurrentId; });
	SelectedPlayerHeadIndex = FMath::Clamp(SelectedPlayerHeadIndex, 0, PlayerHeadPageRows.Num() - 1);
	const FRokPlayerHeadRow& Selected = PlayerHeadPageRows[SelectedPlayerHeadIndex];
	const auto* CurrentHead = Catalog.FindByPredicate([&Profile](const auto& Row) { return Row.Id == Profile.DefaultHeadId; });
	const auto* CurrentFrame = Catalog.FindByPredicate([&Profile](const auto& Row) { return Row.Id == Profile.DefaultFrameId; });

	UBorder* GridPanel = MakeAtlasBorder(TEXT("PlayerHeadGridPanel"), CommonAtlasPath, CommonPanelRect,
		CommonAtlasSize, GridBackgroundSize(), CommonPanelMargin);
	PlaceOnCanvas(ModeCanvas, GridPanel, GridBackgroundPosition(), GridBackgroundSize(), 1);
	// The source pl_right is a layout node, not a dark background image.
	UBorder* PreviewPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlayerHeadPreviewPanel"));
	FSlateBrush NoBrush; NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	PreviewPanel->SetBrush(NoBrush); PreviewPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(ModeCanvas, PreviewPanel, FVector2D(208, 111), FVector2D(327.6, 624), 1);
	const auto StyleText = [](UTextBlock* Text, float Points)
	{
		FSlateFontInfo Font = Text->GetFont(); Font.Size = Points; Text->SetFont(Font);
		Text->SetShadowOffset(FVector2D::ZeroVector);
		Text->SetShadowColorAndOpacity(FLinearColor::Transparent);
	};
	const auto PlaceText = [this](UTextBlock* Text, const FVector2D& Position, const FVector2D& Size, bool CenterVertically)
	{
		UBorder* Region = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("%sRegion"), *Text->GetName()));
		FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType; Region->SetBrush(Empty);
		Region->SetPadding(FMargin(0)); Region->SetHorizontalAlignment(HAlign_Fill);
		Region->SetVerticalAlignment(CenterVertically ? VAlign_Center : VAlign_Top);
		Region->SetVisibility(ESlateVisibility::HitTestInvisible); Region->SetClipping(EWidgetClipping::ClipToBounds); Region->SetContent(Text);
		PlaceOnCanvas(ModeCanvas, Region, Position, Size, 5);
	};
	int32 FreeCount = 0;
	for (const auto& Row : PlayerHeadPageRows) FreeCount += Row.bFree ? 1 : 0;
	UTextBlock* Count = MakeModalText(TEXT("PlayerHeadOwnedCount"), 15, FLinearColor::White);
	StyleText(Count, 15); Count->SetColorAndOpacity(FSlateColor(FLinearColor(.8862745f, .81960785f, .60784316f, 1)));
	Count->SetText(FText::Format(NSLOCTEXT("RokModal", "PlayerHeadCatalogCount", "默认可用：{0} / 目录：{1}"),
		FText::AsNumber(FreeCount), FText::AsNumber(PlayerHeadPageRows.Num())));
	PlaceText(Count, FVector2D(540.35, 127.65), FVector2D(560.5, 42.7), true);

	UScrollBox* GridScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PlayerHeadGridScroll"));
	PlayerHeadGridScroll = GridScroll;
	GridScroll->SetScrollOffset(PlayerHeadScrollOffset);
	GridScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(ModeCanvas, GridScroll, ViewportPosition(), ViewportSize(), 2);
	USizeBox* GridSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PlayerHeadGridSize"));
	GridSize->SetHeightOverride(ContentHeight(PlayerHeadPageRows.Num()));
	GridScroll->AddChild(GridSize);
	UCanvasPanel* Grid = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerHeadGrid"));
	GridSize->SetContent(Grid);
	const auto AddArt = [this](UCanvasPanel* Parent, const FName& Name, const FRokPlayerHeadRow* Row,
		const FVector2D& Position, float Size, int32 Z)
	{
		if (Row && Row->bArtAvailable)
			PlaceOnCanvas(Parent, MakeAtlasImage(Name, *Row->Texture, Row->Rect, Row->AtlasSize, FVector2D(Size)), Position, FVector2D(Size), Z);
	};
	for (int32 Index = 0; Index < PlayerHeadPageRows.Num(); ++Index)
	{
		const auto& Row = PlayerHeadPageRows[Index];
		const FVector2D Position = CellPosition(Index);
		UBorder* Cell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("PlayerHeadCell_%d"), Row.Id));
		Cell->SetBrush(NoBrush); Cell->SetPadding(FMargin(0));
		Cell->SetVisibility(Index == SelectedPlayerHeadIndex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Cell->SetContent(MakeAtlasImage(*FString::Printf(TEXT("PlayerHeadSelectionRing_%d"), Row.Id), SettingAtlasPath,
			SpriteRect(405, 686, 135, 135), SettingAtlasSize, FVector2D(135)));
		// Unity img_selet is sibling 0, behind UI_Model_PlayerHead (sibling 1).
		// Its opaque center is covered by the portrait, leaving the glowing rim.
		PlaceOnCanvas(Grid, Cell, Position, FVector2D(135), 1);
		AddArt(Grid, *FString::Printf(TEXT("PlayerHeadChoiceImage_%d"), Row.Id),
			SelectedPlayerHeadPage == 0 ? &Row : CurrentHead, Position + FVector2D(4.5), 126, 2);
		if (SelectedPlayerHeadPage == 1)
			AddArt(Grid, *FString::Printf(TEXT("PlayerHeadChoiceFrame_%d"), Row.Id), &Row, Position + FVector2D(4.5), 126, 3);
		if (!Row.bArtAvailable)
		{
			UTextBlock* Missing = MakeModalText(*FString::Printf(TEXT("PlayerHeadMissingArt_%d"), Row.Id), 10, FLinearColor::White, false, ETextJustify::Center);
			Missing->SetText(FText::Format(NSLOCTEXT("RokModal", "PlayerHeadArtMissing", "{0}\n图像缺失"), FText::AsNumber(Row.Id)));
			PlaceOnCanvas(Grid, Missing, Position + FVector2D(4.5, 43), FVector2D(126, 42), 4);
		}
		UTextBlock* State = MakeModalText(*FString::Printf(TEXT("PlayerHeadAccess_%d"), Row.Id), 9, FLinearColor::White, false, ETextJustify::Center);
		State->SetText(Row.bFree ? NSLOCTEXT("RokModal", "PlayerHeadFree", "默认可用") : NSLOCTEXT("RokModal", "PlayerHeadUnlockUnknown", "解锁状态待同步"));
		PlaceOnCanvas(Grid, State, Position + FVector2D(4.5, 113), FVector2D(126, 18), 5);
		PlaceOnCanvas(Grid, MakeIndexedHitButton(*FString::Printf(TEXT("PlayerHeadChoiceButton_%d"), Index), 3000 + Index), Position + FVector2D(4.5), FVector2D(126), 8);
	}
	AddArt(ModeCanvas, TEXT("PlayerHeadPreviewImage"), SelectedPlayerHeadPage == 0 ? &Selected : CurrentHead, PreviewPosition(), 195.3f, 3);
	AddArt(ModeCanvas, TEXT("PlayerHeadPreviewFrame"), SelectedPlayerHeadPage == 1 ? &Selected : CurrentFrame, PreviewPosition(), 195.3f, 4);
	if (!Selected.bArtAvailable)
	{
		UTextBlock* Missing = MakeModalText(TEXT("PlayerHeadPreviewMissingArt"), 12, FLinearColor::White, true, ETextJustify::Center);
		Missing->SetText(NSLOCTEXT("RokModal", "PlayerHeadPreviewMissing", "对应图像资源缺失"));
		PlaceOnCanvas(ModeCanvas, Missing, FVector2D(233.8, 269), FVector2D(276, 40), 5);
	}
	UTextBlock* Tag = MakeModalText(TEXT("PlayerHeadPreviewRank"), 15, FLinearColor(0.95f, 0.82f, 0.36f, 1), true, ETextJustify::Center);
	StyleText(Tag, 15); Tag->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Tag->SetText(FText::FromString(Selected.Tag));
	PlaceText(Tag, FVector2D(325.05, 401), FVector2D(94.7, 30), true);
	UTextBlock* Name = MakeModalText(TEXT("PlayerHeadPreviewName"), 19, FLinearColor::White, true, ETextJustify::Center);
	StyleText(Name, 19.5f);
	Name->SetText(FText::FromString(Selected.Name));
	PlaceText(Name, FVector2D(222.55, 437), FVector2D(298.5, 36), true);
	UTextBlock* Description = MakeModalText(TEXT("PlayerHeadPreviewDescription"), 13, FLinearColor(0.78f, 0.78f, 0.75f, 1), false, ETextJustify::Center);
	StyleText(Description, 13.5f); Description->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Description->SetAutoWrapText(true);
	Description->SetText(FText::FromString(Selected.Description));
	PlaceText(Description, FVector2D(233.1, 495.05), FVector2D(277.4, 101.9), false);
	UButton* Confirm = MakeTextAtlasButton(TEXT("PlayerHeadConfirmButton"), CommonAtlasPath, ResourceBlueButtonRect,
		CommonAtlasSize, FVector2D(202, 84), NSLOCTEXT("RokModal", "PlayerHeadConfirm", "使用"), 18, FLinearColor(0.70f, 0.75f, 0.78f, 1));
	// No Role_SetRoleHead adapter exists: even free rows must not fabricate success.
	Confirm->SetIsEnabled(false);
	PlaceOnCanvas(ModeCanvas, Confirm, ConfirmPosition(), FVector2D(202, 84), 6);
	if (!Selected.Tag.IsEmpty())
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("PlayerHeadRankBackground"), SettingAtlasPath,
			SpriteRect(1, 41, 110, 42), SettingAtlasSize, FVector2D(110, 42)), FVector2D(316.8, 395), FVector2D(110, 42), 4);
	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("PlayerHeadPreviewDivider"), CommonAtlasPath,
		SpriteRect(1, 611, 570, 2), CommonAtlasSize, FVector2D(300, 3)), FVector2D(221.8, 475.1), FVector2D(300, 3), 3);
	for (int32 TabIndex = 0; TabIndex < 2; ++TabIndex)
	{
		const bool SelectedTab = TabIndex == SelectedPlayerHeadPage;
		const FVector2D Position = TabPosition(TabIndex);
		const FVector2D BackgroundSize = SelectedTab ? FVector2D(144, 103) : FVector2D(121, 102);
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("PlayerHeadTabBackground_%d"), TabIndex), CommonAtlasPath,
			SelectedTab ? SpriteRect(205, 370, 145, 103) : SpriteRect(205, 266, 121, 102), CommonAtlasSize, BackgroundSize),
			Position + FVector2D(0, SelectedTab ? 0 : .5), BackgroundSize, 4);
		const FIntRect IconRect = TabIndex == 0
			? (SelectedTab ? SpriteRect(1, 1057, 60, 60) : SpriteRect(63, 1925, 60, 60))
			: (SelectedTab ? SpriteRect(1, 1119, 60, 60) : SpriteRect(1, 1181, 60, 60));
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("PlayerHeadTabIcon_%d"), TabIndex), PageAtlasPath,
			IconRect, PageAtlasSize, FVector2D(60)), Position + FVector2D(SelectedTab ? 31.7 : 20.2, 19.6), FVector2D(60), 5);
		UTextBlock* Label = MakeModalText(*FString::Printf(TEXT("PlayerHeadTabLabel_%d"), TabIndex), 14, FLinearColor::White);
		Label->SetText(TabIndex == 0 ? NSLOCTEXT("RokModal", "PlayerHeadTab", "头像") : NSLOCTEXT("RokModal", "PlayerFrameTab", "头像框"));
		Label->SetVisibility(ESlateVisibility::Collapsed); // Source side buttons are icon-only.
		PlaceOnCanvas(ModeCanvas, Label, Position, FVector2D(145, 103), 5);
		UButton* TabButton = MakeIndexedHitButton(*FString::Printf(TEXT("PlayerHeadPageButton_%d"), TabIndex), 3100 + TabIndex);
		TabButton->SetToolTipText(Label->GetText());
		PlaceOnCanvas(ModeCanvas, TabButton, Position, FVector2D(145, 103), 8);
	}
	UTextBlock* Boundary = MakeModalText(TEXT("PlayerHeadServerBoundaryState"), 11, FLinearColor(0.92f, 0.63f, 0.28f, 1), true, ETextJustify::Center);
	Boundary->SetText(NSLOCTEXT("RokModal", "PlayerHeadAuthorityBoundary", "仅预览；解锁列表与更换服务尚未接入，不会修改角色资料。"));
	PlaceOnCanvas(ModeCanvas, Boundary, FVector2D(290, 704), FVector2D(860, 28), 7);
	UE_LOG(LogTemp, Display, TEXT("Rok player head selection: page=%d items=%d selected=%d art=%d authority=server_unchanged"),
		SelectedPlayerHeadPage, PlayerHeadPageRows.Num(), Selected.Id, Selected.bArtAvailable);
}

void URokUnityModalWidget::BuildPlayerPowerDetails()
{
	FRokUnityRuntimePlayerProfileState ProfileState;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (const URokUnityRuntimeStateSubsystem* RuntimeState =
				GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
			RuntimeState && RuntimeState->IsRuntimeStateLoaded())
		{
			ProfileState = RuntimeState->GetPlayerProfileState();
		}
	}

	UBorder* HeaderPanel = MakeAtlasBorder(
		TEXT("PlayerPowerHeaderPanel"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(1016.0f, 120.0f), CommonPanelMargin);
	RokModalNineSlice::SetTint(HeaderPanel, FLinearColor(0.22f, 0.23f, 0.23f, 1.0f));
	PlaceOnCanvas(ModeCanvas, HeaderPanel, FVector2D(212.0f, 95.0f), FVector2D(1016.0f, 120.0f), 1);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(TEXT("PlayerPowerPortrait"), PlayerHeadAtlasPath, PortraitRect, PlayerHeadAtlasSize, FVector2D(92.0f)),
		FVector2D(236.0f, 109.0f), FVector2D(92.0f), 2);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(TEXT("PlayerPowerPortraitFrame"), PlayerHeadAtlasPath, PortraitFrameRect, PlayerHeadAtlasSize, FVector2D(92.0f)),
		FVector2D(236.0f, 109.0f), FVector2D(92.0f), 3);

	UTextBlock* PlayerName = MakeModalText(TEXT("PlayerPowerPlayerName"), 23, FLinearColor::White, true);
	PlayerName->SetText(ProfileState.bHasDisplayName
		? FText::FromString(ProfileState.DisplayName)
		: NSLOCTEXT("RokModal", "PlayerPowerUnknownName", "城主名称未同步"));
	PlaceOnCanvas(ModeCanvas, PlayerName, FVector2D(348.0f, 112.0f), FVector2D(300.0f, 36.0f), 3);

	UButton* RenameButton = MakeAtlasButton(TEXT("PlayerPowerRenameButton"), SettingAtlasPath, RenameRect, SettingAtlasSize, FVector2D(32.0f));
	RenameButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileRenameClicked);
	PlaceOnCanvas(ModeCanvas, RenameButton, FVector2D(642.0f, 112.0f), FVector2D(32.0f), 5);

	UTextBlock* PowerText = MakeModalText(TEXT("PlayerPowerLocalTotal"), 20, FLinearColor(0.92f, 0.83f, 0.56f, 1.0f), true);
	PowerText->SetText(FText::Format(
		NSLOCTEXT("RokModal", "PlayerPowerLocalFormat", "本地预览战力 {0}"),
		FText::AsNumber(ProfileState.LocalPreviewScore)));
	PlaceOnCanvas(ModeCanvas, PowerText, FVector2D(348.0f, 154.0f), FVector2D(330.0f, 32.0f), 3);

	UTextBlock* KillText = MakeModalText(TEXT("PlayerPowerKillCount"), 20, FLinearColor(0.86f, 0.86f, 0.83f, 1.0f), true);
	KillText->SetText(ProfileState.bHasKillCount
		? FText::Format(NSLOCTEXT("RokModal", "PlayerPowerKillFormat", "击杀 {0}"), FText::AsNumber(ProfileState.KillCount))
		: NSLOCTEXT("RokModal", "PlayerPowerKillUnknown", "击杀 —"));
	PlaceOnCanvas(ModeCanvas, KillText, FVector2D(740.0f, 132.0f), FVector2D(330.0f, 34.0f), 3);

	UTextBlock* Boundary = MakeModalText(TEXT("PlayerPowerServerBoundaryState"), 15, FLinearColor(0.92f, 0.63f, 0.28f, 1.0f), true, ETextJustify::Right);
	Boundary->SetText(NSLOCTEXT("RokModal", "PlayerPowerBoundary", "本地基础评分＋当前城市升级增量 · 正式统计待同步"));
	PlaceOnCanvas(ModeCanvas, Boundary, FVector2D(820.0f, 173.0f), FVector2D(378.0f, 26.0f), 4);

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PlayerPowerScroll"));
	Scroll->SetOrientation(Orient_Vertical);
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(ModeCanvas, Scroll, FVector2D(181.0f, 213.5f), FVector2D(1078.0f, 549.5f), 2);

	USizeBox* ContentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PlayerPowerScrollContentSize"));
	ContentSize->SetWidthOverride(1078.0f);
	ContentSize->SetHeightOverride(980.0f);
	Scroll->AddChild(ContentSize);
	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerPowerScrollContent"));
	ContentSize->SetContent(Content);

	auto AddSection = [this, Content](const FName& Prefix, const FText& Title, float Y, const TArray<TPair<FText, FText>>& Rows)
	{
		const float Height = 54.0f + Rows.Num() * 48.0f;
		UBorder* Panel = MakeAtlasBorder(
			*FString::Printf(TEXT("%sPanel"), *Prefix.ToString()),
			CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(1036.0f, Height), CommonPanelMargin);
		RokModalNineSlice::SetTint(Panel, FLinearColor(0.16f, 0.17f, 0.17f, 1.0f));
		PlaceOnCanvas(Content, Panel, FVector2D(21.0f, Y), FVector2D(1036.0f, Height), 1);
		UTextBlock* SectionTitle = MakeModalText(
			*FString::Printf(TEXT("%sTitle"), *Prefix.ToString()), 21, FLinearColor(0.96f, 0.86f, 0.53f, 1.0f), true);
		SectionTitle->SetText(Title);
		PlaceOnCanvas(Content, SectionTitle, FVector2D(48.0f, Y + 10.0f), FVector2D(380.0f, 34.0f), 2);
		for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
		{
			const float RowY = Y + 51.0f + RowIndex * 48.0f;
			UTextBlock* Label = MakeModalText(
				*FString::Printf(TEXT("%sRowLabel_%d"), *Prefix.ToString(), RowIndex), 18, FLinearColor(0.86f, 0.86f, 0.83f, 1.0f));
			Label->SetText(Rows[RowIndex].Key);
			PlaceOnCanvas(Content, Label, FVector2D(64.0f, RowY), FVector2D(620.0f, 32.0f), 2);
			UTextBlock* Value = MakeModalText(
				*FString::Printf(TEXT("%sRowValue_%d"), *Prefix.ToString(), RowIndex), 18, FLinearColor::White, true, ETextJustify::Right);
			Value->SetText(Rows[RowIndex].Value);
			PlaceOnCanvas(Content, Value, FVector2D(720.0f, RowY), FVector2D(286.0f, 32.0f), 2);
		}
	};

	const FText UnknownValue = NSLOCTEXT("RokModal", "PlayerPowerUnknownValue", "—");
	TArray<TPair<FText, FText>> PowerRows;
	PowerRows.Emplace(NSLOCTEXT("RokModal", "LocalBuildingUpgradeScore", "当前城市升级增量"), FText::AsNumber(ProfileState.LocalBuildingUpgradeScore));
	PowerRows.Emplace(NSLOCTEXT("RokModal", "TechnologyPower", "科技战力"), UnknownValue);
	PowerRows.Emplace(NSLOCTEXT("RokModal", "TroopPower", "部队战力"), FText::AsNumber(ProfileState.LocalTroopPower));
	PowerRows.Emplace(NSLOCTEXT("RokModal", "CommanderPower", "统帅战力"), FText::AsNumber(ProfileState.LocalHeroScore));
	AddSection(TEXT("PlayerPowerComposition"), NSLOCTEXT("RokModal", "PlayerPowerCompositionTitle", "战力构成"), 0.0f, PowerRows);

	TArray<TPair<FText, FText>> BattleRows;
	BattleRows.Emplace(NSLOCTEXT("RokModal", "HistoryHighestPower", "历史最高战力"), UnknownValue);
	BattleRows.Emplace(NSLOCTEXT("RokModal", "BattleWins", "胜利次数"), UnknownValue);
	BattleRows.Emplace(NSLOCTEXT("RokModal", "BattleLosses", "失败次数"), UnknownValue);
	BattleRows.Emplace(NSLOCTEXT("RokModal", "DeadTroops", "阵亡部队"), UnknownValue);
	BattleRows.Emplace(NSLOCTEXT("RokModal", "ScoutCount", "侦察次数"), UnknownValue);
	AddSection(TEXT("PlayerPowerBattle"), NSLOCTEXT("RokModal", "PlayerPowerBattleTitle", "战斗统计"), 270.0f, BattleRows);

	TArray<TPair<FText, FText>> ResourceRows;
	ResourceRows.Emplace(NSLOCTEXT("RokModal", "ResourceCollection", "资源采集"), UnknownValue);
	ResourceRows.Emplace(NSLOCTEXT("RokModal", "ResourceAssistance", "资源援助"), UnknownValue);
	ResourceRows.Emplace(NSLOCTEXT("RokModal", "AllianceHelpCount", "联盟帮助"), UnknownValue);
	AddSection(TEXT("PlayerPowerResources"), NSLOCTEXT("RokModal", "PlayerPowerResourcesTitle", "资源与帮助统计"), 565.0f, ResourceRows);
}

void URokUnityModalWidget::BuildPlayerRanking()
{
	FString Error;
	if (!RokLeaderboardCatalog::Load(PlayerRankingRows, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		auto* Missing = MakeModalText(TEXT("PlayerRankingCatalogMissing"), 20, FLinearColor::Red);
		Missing->SetText(NSLOCTEXT("RokModal", "LeaderboardConfigMissing", "排行榜配置缺失"));
		PlaceOnCanvas(ModeCanvas, Missing, FVector2D(400, 350), FVector2D(640, 50));
		return;
	}
	// Source overview is gated by Rank_ShowRankFirst.response. This offline
	// surface is explicitly a configuration preview; it does not claim a response.

	const bool bOverview = SelectedRankingCategory == INDEX_NONE;
	const FVector2D PanelPosition = bOverview
		? FVector2D(200.0f, 91.0f)
		: FVector2D(183.0f, 91.0f);
	const FVector2D PanelSize = bOverview
		? FVector2D(1040.0f, 664.0f)
		: FVector2D(1074.0f, 664.0f);

	if (bOverview)
	{
		using namespace RokLeaderboardOverviewLayout;
		// The source pl_Total is a layout group, not an extra dark panel.
		auto* Overview = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerRankingMainPanel"));
		PlaceOnCanvas(ModeCanvas, Overview, PanelPosition, PanelSize, 1);
		auto* StateText = MakeModalText(TEXT("PlayerRankingServerBoundaryState"), 12, FLinearColor(0.92f, 0.67f, 0.31f), true, ETextJustify::Center);
		StateText->SetText(NSLOCTEXT("RokModal", "PlayerRankingCategoryBoundary", "排行榜配置预览 · 榜单尚未同步"));
		PlaceOnCanvas(ModeCanvas, StateText, FVector2D(250, 118), FVector2D(940, 24), 4);
		for (int32 Index = 0; Index < PlayerRankingRows.Num(); ++Index)
		{
			const FVector2D Tile = TilePosition(Index);
			auto* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("PlayerRankingCategoryTile_%d"), Index));
			FSlateBrush NoBackground; NoBackground.DrawAs = ESlateBrushDrawType::NoDrawType;
			Background->SetBrush(NoBackground); Background->SetPadding(FMargin(0)); Background->SetVisibility(ESlateVisibility::HitTestInvisible);
			auto* Slices = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("PlayerRankingCategorySlices_%d"), Index));
			Background->SetContent(Slices);
			for (int32 SliceIndex = 0; SliceIndex < 9; ++SliceIndex)
			{
				const auto Slice = BackgroundSlice(SliceIndex);
				PlaceOnCanvas(Slices, MakeAtlasImage(*FString::Printf(TEXT("PlayerRankingCategorySlice_%d_%d"), Index, SliceIndex),
					CommonAtlasPath, Slice.Rect, CommonAtlasSize, Slice.Size), Slice.Position, Slice.Size);
			}
			PlaceOnCanvas(Overview, Background, Tile + BackgroundPosition(), BackgroundSize(), 1);
			if (PlayerRankingRows[Index].HasIcon())
				PlaceOnCanvas(Overview, MakeAtlasImage(*FString::Printf(TEXT("PlayerRankingCategoryIcon_%d"), Index),
					PlayerUiAtlasPath, PlayerRankingRows[Index].IconRect, PlayerUiAtlasSize, IconSize()), Tile + IconPosition(), IconSize(), 2);
			auto AddSourceText = [this, Overview, Tile, Index](const TCHAR* Prefix, const FString& Value, float Pixels,
				const FVector2D& Position, const FVector2D& Size, EVerticalAlignment VerticalAlignment)
			{
				const FName Name(*FString::Printf(TEXT("%s_%d"), Prefix, Index));
				auto* Text = MakeModalText(Name, 15, FLinearColor::White, Pixels == 20, ETextJustify::Left);
				auto Font = Text->GetFont(); Font.Size = RokLeaderboardOverviewLayout::FontPoints(Pixels); Text->SetFont(Font);
				Text->SetShadowOffset(FVector2D::ZeroVector); Text->SetShadowColorAndOpacity(FLinearColor::Transparent);
				Text->SetAutoWrapText(true); Text->SetWrapTextAt(Size.X); Text->SetText(FText::FromString(Value));
				auto* Region = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("%sRegion_%d"), Prefix, Index));
				FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType; Region->SetBrush(Empty);
				Region->SetPadding(FMargin(0)); Region->SetHorizontalAlignment(HAlign_Fill); Region->SetVerticalAlignment(VerticalAlignment);
				Region->SetVisibility(ESlateVisibility::HitTestInvisible); Region->SetContent(Text);
				PlaceOnCanvas(Overview, Region, Tile + Position, Size, 3);
			};
			// RefreshEntryBtn(null rankInfo) uses crossBar for both score and owner.
			// Source title/mid Arabic flags=10 become MiddleLeft; owner becomes UpperLeft.
			AddSourceText(TEXT("PlayerRankingCategoryLabel"), PlayerRankingRows[Index].Name, 20, TitlePosition(), TitleSize(), VAlign_Center);
			AddSourceText(TEXT("PlayerRankingCategoryState"), TEXT("--"), 18, ScorePosition(), ScoreSize(), VAlign_Center);
			AddSourceText(TEXT("PlayerRankingCategoryOwner"), TEXT("--"), 18, OwnerPosition(), OwnerSize(), VAlign_Top);
			PlaceOnCanvas(Overview, MakeIndexedHitButton(*FString::Printf(TEXT("PlayerRankingCategoryButton_%d"), Index), 3200 + Index),
				Tile, FVector2D(340, 144), 8);
		}
		return;
	}
	UBorder* MainPanel = MakeAtlasBorder(TEXT("PlayerRankingDetailPanel"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, PanelSize, CommonPanelMargin);
	RokModalNineSlice::SetTint(MainPanel, FLinearColor(0.18f, 0.19f, 0.19f, 1.f));
	PlaceOnCanvas(ModeCanvas, MainPanel, PanelPosition, PanelSize, 1);

	const int32 CategoryIndex = FMath::Clamp(SelectedRankingCategory, 0, PlayerRankingRows.Num() - 1);
	PlaceOnCanvas(ModeCanvas, MakeIndexedHitButton(TEXT("PlayerRankingBackButton"), 3299), FVector2D(203.0f, 107.0f), FVector2D(108.0f, 54.0f), 8);
	UTextBlock* BackLabel = MakeModalText(TEXT("PlayerRankingBackLabel"), 18, FLinearColor(0.94f, 0.84f, 0.53f, 1.0f), true, ETextJustify::Center);
	BackLabel->SetText(NSLOCTEXT("RokModal", "RankingBack", "‹ 返回"));
	PlaceOnCanvas(ModeCanvas, BackLabel, FVector2D(203.0f, 113.0f), FVector2D(108.0f, 38.0f), 6);

	if (PlayerRankingRows[CategoryIndex].HasIcon())
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(TEXT("PlayerRankingDetailIcon"), PlayerUiAtlasPath, PlayerRankingRows[CategoryIndex].IconRect, PlayerUiAtlasSize, FVector2D(76.0f, 90.0f)),
			FVector2D(327.0f, 108.0f),
			FVector2D(76.0f, 90.0f),
			3);
	}
	UTextBlock* DetailTitle = MakeModalText(TEXT("PlayerRankingDetailTitle"), 25, FLinearColor::White, true);
	DetailTitle->SetText(FText::FromString(PlayerRankingRows[CategoryIndex].Name));
	PlaceOnCanvas(ModeCanvas, DetailTitle, FVector2D(421.0f, 119.0f), FVector2D(420.0f, 42.0f), 5);

	UTextBlock* SelfRank = MakeModalText(TEXT("PlayerRankingSelfRank"), 18, FLinearColor(0.94f, 0.84f, 0.53f, 1.0f), true, ETextJustify::Right);
	SelfRank->SetText(NSLOCTEXT("RokModal", "RankingSelfUnknown", "我的排名  —"));
	PlaceOnCanvas(ModeCanvas, SelfRank, FVector2D(883.0f, 124.0f), FVector2D(290.0f, 34.0f), 5);

	UBorder* Header = MakeAtlasBorder(
		TEXT("PlayerRankingListHeader"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(974.0f, 52.0f), CommonPanelMargin);
	RokModalNineSlice::SetTint(Header, FLinearColor(0.30f, 0.29f, 0.24f, 1.0f));
	PlaceOnCanvas(ModeCanvas, Header, FVector2D(216.0f, 207.0f), FVector2D(1008.0f, 52.0f), 3);
	static const FText ColumnLabels[] = {
		NSLOCTEXT("RokModal", "RankingColumnRank", "排名"),
		NSLOCTEXT("RokModal", "RankingColumnName", "名称"),
		NSLOCTEXT("RokModal", "RankingColumnValue", "数值")};
	static const float ColumnX[] = {233.0f, 423.0f, 943.0f};
	static const float ColumnWidth[] = {160.0f, 480.0f, 220.0f};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UTextBlock* Column = MakeModalText(
			*FString::Printf(TEXT("PlayerRankingColumn_%d"), Index), 18, FLinearColor(0.95f, 0.92f, 0.83f, 1.0f), true,
			Index == 2 ? ETextJustify::Right : ETextJustify::Left);
		Column->SetText(Index == 2 ? FText::FromString(PlayerRankingRows[CategoryIndex].Columns.IsValidIndex(0) ? PlayerRankingRows[CategoryIndex].Columns[0] : FString()) : ColumnLabels[Index]);
		PlaceOnCanvas(ModeCanvas, Column, FVector2D(ColumnX[Index], 216.0f), FVector2D(ColumnWidth[Index], 32.0f), 5);
	}

	UTextBlock* EmptyState = MakeModalText(TEXT("PlayerRankingEmptyState"), 24, FLinearColor(0.76f, 0.76f, 0.73f, 1.0f), false, ETextJustify::Center);
	EmptyState->SetAutoWrapText(true);
	EmptyState->SetText(NSLOCTEXT("RokModal", "RankingServerEmpty", "尚未同步排行榜数据"));
	PlaceOnCanvas(ModeCanvas, EmptyState, FVector2D(313.0f, 395.0f), FVector2D(814.0f, 90.0f), 5);
	UTextBlock* ProtocolState = MakeModalText(TEXT("PlayerRankingServerBoundaryState"), 15, FLinearColor(0.92f, 0.63f, 0.28f, 1.0f), true, ETextJustify::Center);
	ProtocolState->SetText(NSLOCTEXT("RokModal", "RankingProtocolBoundary", "配置预览 · 榜单服务尚未接入"));
	PlaceOnCanvas(ModeCanvas, ProtocolState, FVector2D(333.0f, 637.0f), FVector2D(774.0f, 28.0f), 6);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok player ranking screen: leaderboard_id=%d config_preview=1 authority=server_rows_not_synced."),
		PlayerRankingRows[CategoryIndex].Id);
}

void URokUnityModalWidget::BuildCivilizationSelection()
{
	TArray<FRokUnityCatalogCivilization> Civilizations =
		LoadCivilizationRowsForUi(GetGameInstance());
	FRokUnityRuntimePlayerProfileState ProfileState;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (const URokUnityRuntimeStateSubsystem* RuntimeState =
				GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>();
			RuntimeState && RuntimeState->IsRuntimeStateLoaded())
		{
			ProfileState = RuntimeState->GetPlayerProfileState();
		}
	}

	// CreateCharMediator sorts by ifLock only. Keep unlocked rows first and use
	// source IDs as the deterministic order inside each equal comparator group.
	Civilizations.Sort(
		[](const FRokUnityCatalogCivilization& Left,
			const FRokUnityCatalogCivilization& Right)
		{
			return Left.bLocked == Right.bLocked
				? Left.Id < Right.Id
				: !Left.bLocked;
		});

	const FRokUnityCatalogCivilization* SelectedCivilization = nullptr;
	for (const FRokUnityCatalogCivilization& Civilization : Civilizations)
	{
		if (Civilization.Id == SelectedCivilizationId)
		{
			SelectedCivilization = &Civilization;
			break;
		}
	}
	if (!SelectedCivilization)
	{
		for (const FRokUnityCatalogCivilization& Civilization : Civilizations)
		{
			if (!Civilization.bLocked)
			{
				SelectedCivilizationId = Civilization.Id;
				SelectedCivilization = &Civilization;
				break;
			}
		}
	}

	const int32 BackgroundIndex = SelectedCivilization
		? FindCivilizationBackgroundIndex(SelectedCivilization->Id)
		: INDEX_NONE;
	if (BackgroundIndex != INDEX_NONE)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				TEXT("CivilizationSelectionBackground"),
				CivilizationBackgroundAtlasPath,
				CivilizationBackgroundRects[BackgroundIndex],
				CivilizationBackgroundAtlasSize,
				FVector2D(1440.0f, 810.0f)),
			FVector2D::ZeroVector,
			FVector2D(1440.0f, 810.0f),
			0);
	}

	// CreateCharMediator loads Civilization.heroRes, not a generic portrait.
	// Validate the source mapping before sharing the exported hero-id mesh clips.
	if (SelectedCivilization && !SelectedCivilization->bLocked
		&& SelectedCivilization->HeroResourceKey == FString::Printf(TEXT("Hero_%d_SD"), SelectedCivilization->InitialHeroId))
	{
		if (!CivilizationHero || CivilizationHeroId != SelectedCivilization->InitialHeroId)
		{
			CivilizationHeroId = SelectedCivilization->InitialHeroId;
			CivilizationHeroAnimationTime = 0;
			if (!CivilizationHero)
			{
				CivilizationHero = WidgetTree->ConstructWidget<URokCommanderMeshWidget>(URokCommanderMeshWidget::StaticClass(), TEXT("CivilizationAnimatedHero"));
			}
			if (!CivilizationHero->LoadHero(CivilizationHeroId))
			{
				UE_LOG(LogTemp, Warning, TEXT("Civilization source hero mesh missing: %s"), *SelectedCivilization->HeroResourceKey);
				CivilizationHero = nullptr;
			}
		}
		if (CivilizationHero)
		{
			CivilizationHero->SetCivilizationProjection(SelectedCivilization->HeroAboutOffset);
			PlaceOnCanvas(ModeCanvas, CivilizationHero, FVector2D::ZeroVector, FVector2D(1440, 810), 1);
		}
	}
	// CreateCharView img_charCountryFlag: img_loading_flagBg, Simple image,
	// no nine-slice border. Its 823x810 source sprite is drawn at 823x819.
	UImage* CountryFlag = MakeAtlasImage(TEXT("CivilizationCountryFlag"),
		TEXT("/Game/RokPrototype/UI/BuildCityAtlases/rok_buildcity_atlas_login_loading.rok_buildcity_atlas_login_loading"),
		SpriteRect(1, 1237, 823, 810), FIntPoint(1024, 2048), FVector2D(823, 819));
	PlaceOnCanvas(ModeCanvas, CountryFlag, FVector2D(563, -10), FVector2D(823, 819), 1);

	UButton* BackButton = MakeAtlasButton(
		TEXT("CivilizationSelectionBackButton"), CommonAtlasPath, CloseRect,
		CommonAtlasSize, FVector2D(69.0f));
	BackButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
	PlaceOnCanvas(ModeCanvas, BackButton, FVector2D(16.0f, 6.0f), FVector2D(69.0f), 12);

	UTextBlock* PageTitle = MakeModalText(
		TEXT("CivilizationSelectionTitle"), 29, FLinearColor::White, true);
	PageTitle->SetText(NSLOCTEXT("RokModal", "CivilizationSelectionTitleText", "选择文明"));
	PlaceOnCanvas(ModeCanvas, PageTitle, FVector2D(101.0f, 17.0f), FVector2D(300.0f, 45.0f), 12);

	if (SelectedCivilization)
	{
		const int32 VisualIndex = FindCivilizationVisualIndex(SelectedCivilization->Id);
		const int32 PreviewIndex = FindCivilizationUnlockedPreviewIndex(SelectedCivilization->Id);
		if (PreviewIndex != INDEX_NONE)
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					TEXT("CivilizationCityPreview"), CivilizationImageAtlasPath,
					CivilizationSceneRects[PreviewIndex], CivilizationImageAtlasSize,
					FVector2D(734.0f, 261.0f)),
				FVector2D(609.5f, 118.0f), FVector2D(734.0f, 261.0f), 2);
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					TEXT("CivilizationSoldierPreview"), CivilizationImageAtlasPath,
					CivilizationSoldierRects[PreviewIndex], CivilizationImageAtlasSize,
					FVector2D(206.0f, 174.0f)),
				FVector2D(1182.39f, 225.5f), FVector2D(206.0f, 174.0f), 3);
		}
		if (VisualIndex != INDEX_NONE)
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					TEXT("CivilizationSelectedMark"), CivilizationAtlasPath,
					CivilizationMarkRects[VisualIndex], CivilizationAtlasSize,
					FVector2D(53.0f, 51.0f)),
				FVector2D(1243.5f, 50.0f), FVector2D(53.0f, 51.0f), 5);
		}

		UTextBlock* CivilizationName = MakeModalText(
			TEXT("CivilizationSelectedName"), 34,
			FLinearColor(1.0f, 0.82f, 0.32f, 1.0f), true, ETextJustify::Center);
		CivilizationName->SetText(FText::FromString(SelectedCivilization->DisplayNameZhCn));
		PlaceOnCanvas(ModeCanvas, CivilizationName, FVector2D(759.0f, 54.0f), FVector2D(438.0f, 52.0f), 6);

		UTextBlock* BonusTitle = MakeModalText(
			TEXT("CivilizationBonusTitle"), 25,
			FLinearColor(1.0f, 0.84f, 0.37f, 1.0f), true, ETextJustify::Center);
		BonusTitle->SetText(FText::FromString(SelectedCivilization->BonusNameZhCn));
		PlaceOnCanvas(ModeCanvas, BonusTitle, FVector2D(854.25f, 409.5f), FVector2D(465.7f, 30.0f), 6);

		UTextBlock* BonusDescription = MakeModalText(
			TEXT("CivilizationBonusDescription"), 19,
			FLinearColor(0.93f, 0.93f, 0.90f, 1.0f), false, ETextJustify::Center);
		BonusDescription->SetAutoWrapText(true);
		BonusDescription->SetText(FText::FromString(
			FormatCivilizationBonusDescription(*SelectedCivilization)));
		PlaceOnCanvas(ModeCanvas, BonusDescription, FVector2D(621.03f, 451.0f), FVector2D(701.5f, 67.8f), 6);

		UTextBlock* HeroState = MakeModalText(
			TEXT("CivilizationInitialHero"), 17,
			FLinearColor(0.82f, 0.84f, 0.84f, 1.0f), true, ETextJustify::Center);
		FString InitialHeroName;
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (const URokUnityGameplayCatalogSubsystem* Catalog =
					GameInstance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
				Catalog && Catalog->IsCatalogLoaded())
			{
				FRokUnityCatalogHero Hero;
				if (Catalog->GetHero(SelectedCivilization->InitialHeroId, Hero))
				{
					InitialHeroName = Hero.DisplayNameZhCn;
				}
			}
		}
		HeroState->SetText(FText::Format(
			NSLOCTEXT("RokModal", "CivilizationInitialHeroFormat", "初始统帅：{0}"),
			InitialHeroName.IsEmpty()
				? FText::FromString(FString::FromInt(SelectedCivilization->InitialHeroId))
				: FText::FromString(InitialHeroName)));
		PlaceOnCanvas(ModeCanvas, HeroState, FVector2D(840.0f, 536.0f), FVector2D(310.0f, 30.0f), 6);

		const bool bSameAsCurrent = ProfileState.bHasCivilization
			&& ProfileState.CivilizationId == SelectedCivilization->Id;
		UButton* ContinueButton = MakeTextAtlasButton(
			TEXT("CivilizationContinueButton"), CommonAtlasPath,
			ResourceGoldButtonRect, CommonAtlasSize, FVector2D(202.0f, 84.0f),
			bSameAsCurrent
				? NSLOCTEXT("RokModal", "CivilizationCurrent", "当前文明")
				: NSLOCTEXT("RokModal", "CivilizationChange", "更换文明"),
			24, FLinearColor::White);
		ContinueButton->SetIsEnabled(!SelectedCivilization->bLocked && !bSameAsCurrent);
		if (ContinueButton->GetIsEnabled())
		{
			ContinueButton->OnClicked.AddDynamic(
				this, &URokUnityModalWidget::HandleCivilizationContinueClicked);
		}
		PlaceOnCanvas(ModeCanvas, ContinueButton, FVector2D(1127.5f, 521.5f), FVector2D(202.0f, 84.0f), 8);
	}

	UBorder* ListShade = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CivilizationListShade"));
	ListShade->SetPadding(FMargin(0.0f));
	ListShade->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.025f, 0.88f));
	ListShade->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(ModeCanvas, ListShade, FVector2D(0.0f, 640.7f), FVector2D(1440.0f, 169.3f), 4);

	CivilizationSelectionPulse = nullptr;
	for (int32 Index = 0; Index < Civilizations.Num() && Index < 7; ++Index)
	{
		const FRokUnityCatalogCivilization& Civilization = Civilizations[Index];
		const int32 VisualIndex = FindCivilizationVisualIndex(Civilization.Id);
		if (VisualIndex == INDEX_NONE)
		{
			continue;
		}
		const float ItemX = 1241.0f - Index * 199.0f;
		const FVector2D FlagPosition(ItemX + 14.5f, 662.85f - (Civilization.Id == SelectedCivilizationId ? 28.0f : 0.0f));
		if (Civilization.Id == SelectedCivilizationId)
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					*FString::Printf(TEXT("CivilizationSelectedFrame_%d"), Civilization.Id),
					CivilizationAtlasPath, CivilizationSelectRect,
					CivilizationAtlasSize, FVector2D(208.0f, 204.0f)),
				FVector2D(ItemX - 4.5f, 616.85f), FVector2D(208.0f, 204.0f), 5);
			CivilizationSelectionPulse = MakeAtlasImage(TEXT("CivilizationSelectionPulse"), CivilizationAtlasPath,
				CivilizationSelectRect, CivilizationAtlasSize, FVector2D(208, 204));
			CivilizationSelectionPulse->SetRenderTransformPivot(FVector2D(0.5, 0));
			PlaceOnCanvas(ModeCanvas, CivilizationSelectionPulse,
				FVector2D(ItemX - 4.5f, 616.85f), FVector2D(208, 204), 5);
		}
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				*FString::Printf(TEXT("CivilizationFlag_%d"), Civilization.Id),
				CivilizationAtlasPath, CivilizationFlagRects[VisualIndex],
				CivilizationAtlasSize, FVector2D(170.0f, 140.0f)),
			FlagPosition, FVector2D(170.0f, 140.0f), 6);

		const bool bSelected = Civilization.Id == SelectedCivilizationId;
		UTextBlock* CountryName = MakeModalText(
			// Unity 22px -> Slate font points (96dpi): 22 * 72 / 96.
			*FString::Printf(TEXT("CivilizationChoiceName_%d"), Civilization.Id), 17,
			bSelected ? FLinearColor(0.3254902f, 0.2784314f, 0.14117648f, 1)
				: FLinearColor(0.8509804f, 0.83137256f, 0.7294118f, 1), true, ETextJustify::Center);
		CountryName->SetText(FText::FromString(Civilization.DisplayNameZhCn));
		CountryName->SetShadowOffset(FVector2D::ZeroVector);
		// CreateCharFlagItem labels: center anchor, Y -72 / -62.7,
		// 160x30, within the source 199x180 item at Y 634.85.
		PlaceOnCanvas(ModeCanvas, CountryName, FVector2D(ItemX + 19.5f, bSelected ? 772.55f : 781.85f), FVector2D(160, 30), 9);

		if (Civilization.bLocked)
		{
			UBorder* LockedShade = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(),
				*FString::Printf(TEXT("CivilizationLockedShade_%d"), Civilization.Id));
			LockedShade->SetPadding(FMargin(0.0f));
			LockedShade->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.62f));
			LockedShade->SetVisibility(ESlateVisibility::HitTestInvisible);
			PlaceOnCanvas(ModeCanvas, LockedShade, FlagPosition, FVector2D(170.0f, 140.0f), 7);
			UTextBlock* LockedText = MakeModalText(
				*FString::Printf(TEXT("CivilizationLockedState_%d"), Civilization.Id),
				17, FLinearColor(0.84f, 0.84f, 0.82f, 1.0f), true, ETextJustify::Center);
			LockedText->SetText(NSLOCTEXT("RokModal", "CivilizationLocked", "尚未开放"));
			PlaceOnCanvas(ModeCanvas, LockedText, FlagPosition + FVector2D(12.0f, 54.0f), FVector2D(146.0f, 30.0f), 8);
		}
		else
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeIndexedHitButton(
					*FString::Printf(TEXT("CivilizationChoiceButton_%d"), Civilization.Id),
					3300 + Index),
				FlagPosition, FVector2D(170.0f, 140.0f), 10);
		}
	}

	UTextBlock* SelectionState = MakeModalText(
		TEXT("CivilizationSelectionServerBoundaryState"), 16,
		FLinearColor(0.98f, 0.82f, 0.42f, 1.0f), true, ETextJustify::Center);
	SelectionState->SetText(Civilizations.Num() == 7
		? NSLOCTEXT("RokModal", "CivilizationSelectionHint", "选择文明并查看加成 · 最终更换需要服务器确认")
		: NSLOCTEXT("RokModal", "CivilizationCatalogUnavailable", "文明配置尚未载入"));
	PlaceOnCanvas(ModeCanvas, SelectionState, FVector2D(638.13f, 619.5f), FVector2D(684.67f, 30.0f), 11);

	// Only sv_list_country descendants belong to the list animation. Keep
	// background, details, close and confirmation controls stationary.
	const TArray<UWidget*> Children = ModeCanvas->GetAllChildren();
	CivilizationListRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CivilizationListRoot"));
	CivilizationListRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ModeCanvas, CivilizationListRoot, FVector2D::ZeroVector, FVector2D(1440, 810), 10);
	for (UWidget* Child : Children)
	{
		const FString Name = Child->GetName();
		if (Child != CivilizationSelectionPulse && !Name.StartsWith(TEXT("CivilizationFlag_")) && !Name.StartsWith(TEXT("CivilizationSelectedFrame_"))
			&& !Name.StartsWith(TEXT("CivilizationLockedShade_")) && !Name.StartsWith(TEXT("CivilizationLockedState_"))
			&& !Name.StartsWith(TEXT("CivilizationChoiceName_"))
			&& !Name.StartsWith(TEXT("CivilizationChoiceButton_"))) continue;
		const auto* OldSlot = CastChecked<UCanvasPanelSlot>(Child->Slot);
		const FVector2D Position = OldSlot->GetPosition();
		const FVector2D Size = OldSlot->GetSize();
		const int32 ZOrder = OldSlot->GetZOrder();
		Child->RemoveFromParent();
		PlaceOnCanvas(CivilizationListRoot, Child, Position, Size, ZOrder);
	}

	// The flag's descendants share its CanvasGroup and motion; country
	// selection, page back button and background are outside this group.
	CivilizationDetailRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CivilizationDetailRoot"));
	CivilizationDetailRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const TArray<UWidget*> DetailCandidates = ModeCanvas->GetAllChildren();
	PlaceOnCanvas(ModeCanvas, CivilizationDetailRoot, FVector2D::ZeroVector, FVector2D(1440, 810), 2);
	for (UWidget* Child : DetailCandidates)
	{
		const FString Name = Child->GetName();
		if (Name != TEXT("CivilizationCountryFlag") && Name != TEXT("CivilizationCityPreview")
			&& Name != TEXT("CivilizationSoldierPreview") && Name != TEXT("CivilizationSelectedMark")
			&& Name != TEXT("CivilizationSelectedName") && Name != TEXT("CivilizationBonusTitle")
			&& Name != TEXT("CivilizationBonusDescription") && Name != TEXT("CivilizationInitialHero")
			&& Name != TEXT("CivilizationContinueButton")) continue;
		const auto* OldSlot = CastChecked<UCanvasPanelSlot>(Child->Slot);
		const FVector2D Position = OldSlot->GetPosition();
		const FVector2D Size = OldSlot->GetSize();
		const int32 ZOrder = OldSlot->GetZOrder();
		Child->RemoveFromParent();
		PlaceOnCanvas(CivilizationDetailRoot, Child, Position, Size, ZOrder);
	}

	// CreateCharView uses an Expand canvas: background stretches, country
	// list stays at the bottom, and pl_charChoose content is center-anchored.
	// Keep source sizes in design units; anchors absorb the expanded viewport.
	for (UWidget* Child : ModeCanvas->GetAllChildren())
	{
		auto* ChildSlot = CastChecked<UCanvasPanelSlot>(Child->Slot);
		const FName Name = Child->GetFName();
		if (Name == TEXT("CivilizationSelectionBackground"))
		{
			ChildSlot->SetAnchors(FAnchors(0, 0, 1, 1));
			ChildSlot->SetOffsets(FMargin(0));
		}
		else if (Child == CivilizationListRoot)
		{
			ChildSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			ChildSlot->SetPosition(FVector2D(-720, -810));
		}
		else if (Name == TEXT("CivilizationListShade"))
		{
			ChildSlot->SetAnchors(FAnchors(0, 1, 1, 1));
			ChildSlot->SetOffsets(FMargin(0, -169.3f, 0, 169.3f));
		}
		else if (Name != TEXT("CivilizationSelectionBackButton") && Name != TEXT("CivilizationSelectionTitle"))
		{
			const FVector2D Position = ChildSlot->GetPosition();
			ChildSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			ChildSlot->SetPosition(Position - FVector2D(720, 405));
		}
	}
}

void URokUnityModalWidget::BuildCivilizationChangeConfirmation()
{
	FRokUnityCatalogCivilization Civilization;
	bool bFoundCivilization = false;
	const TArray<FRokUnityCatalogCivilization> Civilizations =
		LoadCivilizationRowsForUi(GetGameInstance());
	for (const FRokUnityCatalogCivilization& Candidate : Civilizations)
	{
		if (Candidate.Id == SelectedCivilizationId)
		{
			Civilization = Candidate;
			bFoundCivilization = true;
			break;
		}
	}

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), TEXT("CivilizationConfirmationScroll"));
	Scroll->SetOrientation(Orient_Vertical);
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(ModeCanvas, Scroll, FVector2D(239.35f, 122.3f), FVector2D(959.3f, 504.8f), 3);

	USizeBox* ContentSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("CivilizationConfirmationContentSize"));
	ContentSize->SetWidthOverride(959.3f);
	ContentSize->SetHeightOverride(760.0f);
	Scroll->AddChild(ContentSize);
	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("CivilizationConfirmationContent"));
	ContentSize->SetContent(Content);

	UTextBlock* NameText = MakeModalText(
		TEXT("CivilizationConfirmationName"), 30,
		FLinearColor(1.0f, 0.83f, 0.34f, 1.0f), true, ETextJustify::Center);
	NameText->SetText(bFoundCivilization
		? FText::FromString(Civilization.DisplayNameZhCn)
		: NSLOCTEXT("RokModal", "CivilizationConfirmationMissing", "文明配置不可用"));
	PlaceOnCanvas(Content, NameText, FVector2D(80.0f, 18.0f), FVector2D(799.0f, 46.0f), 2);

	UTextBlock* Warning = MakeModalText(
		TEXT("CivilizationConfirmationWarning"), 20,
		FLinearColor(0.95f, 0.92f, 0.84f, 1.0f), false, ETextJustify::Center);
	Warning->SetAutoWrapText(true);
	Warning->SetText(NSLOCTEXT(
		"RokModal", "CivilizationConfirmationWarningText",
		"更换文明会影响文明加成和特色兵种。提交前必须完成下列权威检查。"));
	PlaceOnCanvas(Content, Warning, FVector2D(68.0f, 72.0f), FVector2D(823.0f, 58.0f), 2);

	static const FText CheckLabels[] = {
		NSLOCTEXT("RokModal", "CivilizationCheckMarch", "所有部队均在城市内"),
		NSLOCTEXT("RokModal", "CivilizationCheckBattle", "城市未处于战斗状态"),
		NSLOCTEXT("RokModal", "CivilizationCheckTraining", "没有正在训练的部队"),
		NSLOCTEXT("RokModal", "CivilizationCheckHospital", "医院没有重伤单位"),
		NSLOCTEXT("RokModal", "CivilizationCheckCost", "文明更换道具 x1，或宝石 10,000"),
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(CheckLabels); ++Index)
	{
		const float Y = 150.0f + Index * 62.0f;
		UBorder* Row = MakeAtlasBorder(
			*FString::Printf(TEXT("CivilizationConfirmationCheckRow_%d"), Index),
			CommonAtlasPath, CommonPanelRect, CommonAtlasSize,
			FVector2D(860.0f, 52.0f), CommonPanelMargin);
		RokModalNineSlice::SetTint(Row, FLinearColor(0.16f, 0.17f, 0.17f, 0.94f));
		PlaceOnCanvas(Content, Row, FVector2D(49.0f, Y), FVector2D(860.0f, 52.0f), 1);
		UTextBlock* Label = MakeModalText(
			*FString::Printf(TEXT("CivilizationConfirmationCheckLabel_%d"), Index),
			18, FLinearColor(0.92f, 0.92f, 0.89f, 1.0f), true);
		Label->SetText(CheckLabels[Index]);
		PlaceOnCanvas(Content, Label, FVector2D(73.0f, Y + 10.0f), FVector2D(560.0f, 32.0f), 2);
		UTextBlock* State = MakeModalText(
			*FString::Printf(TEXT("CivilizationConfirmationCheckState_%d"), Index),
			16, FLinearColor(0.95f, 0.63f, 0.28f, 1.0f), true, ETextJustify::Right);
		State->SetText(NSLOCTEXT("RokModal", "CivilizationCheckUnsynced", "等待服务器校验"));
		PlaceOnCanvas(Content, State, FVector2D(642.0f, Y + 11.0f), FVector2D(238.0f, 30.0f), 2);
	}

	UTextBlock* BonusText = MakeModalText(
		TEXT("CivilizationConfirmationBonus"), 18,
		FLinearColor(0.82f, 0.82f, 0.79f, 1.0f), false, ETextJustify::Center);
	BonusText->SetAutoWrapText(true);
	BonusText->SetText(bFoundCivilization
		? FText::FromString(FormatCivilizationBonusDescription(Civilization))
		: FText::GetEmpty());
	PlaceOnCanvas(Content, BonusText, FVector2D(55.0f, 480.0f), FVector2D(849.0f, 76.0f), 2);

	UTextBlock* Boundary = MakeModalText(
		TEXT("CivilizationConfirmationServerBoundaryState"), 17,
		FLinearColor(0.96f, 0.53f, 0.28f, 1.0f), true, ETextJustify::Center);
	Boundary->SetText(NSLOCTEXT(
		"RokModal", "CivilizationConfirmationBoundary",
		"Role_ChangeCivilization 未接入：不会扣除道具或宝石，也不会修改角色文明。"));
	PlaceOnCanvas(Content, Boundary, FVector2D(44.0f, 595.0f), FVector2D(871.0f, 40.0f), 2);

	UButton* ConfirmButton = MakeTextAtlasButton(
		TEXT("CivilizationSubmitButton"), CommonAtlasPath,
		ResourceGoldButtonRect, CommonAtlasSize, FVector2D(202.0f, 73.0f),
		NSLOCTEXT("RokModal", "CivilizationSubmit", "确认"),
		24, FLinearColor(0.72f, 0.72f, 0.70f, 1.0f));
	ConfirmButton->SetIsEnabled(false);
	PlaceOnCanvas(ModeCanvas, ConfirmButton, FVector2D(619.0f, 635.3f), FVector2D(202.0f, 73.0f), 6);
}

void URokUnityModalWidget::BuildChangeName()
{
	using namespace RokChangeNameLayout;
	UTextBlock* CurrentNameText = MakeModalText(
		TEXT("ChangeNameCurrentName"),
		19,
		FLinearColor(0.92f, 0.86f, 0.72f, 1.0f),
		true,
		ETextJustify::Center);
	CurrentNameText->SetText(bChangeNameHasCurrentName
		? FText::Format(
			NSLOCTEXT("RokModal", "ChangeNameCurrentNameValue", "当前名称：{0}"),
			FText::FromString(ChangeNameCurrentName))
		: NSLOCTEXT(
			"RokModal",
			"ChangeNameCurrentNameUnavailable",
			"当前名称：--（本地资料未载入）"));
	PlaceOnCanvas(
		ModeCanvas,
		CurrentNameText,
		FVector2D(329.0f, 202.0f),
		FVector2D(782.0f, 30.0f),
		3);

	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(
			TEXT("ChangeNameInputBackground"),
			CommonAtlasPath,
			CommonPanelRect,
			CommonAtlasSize,
			InputSize(),
			CommonPanelMargin),
		InputPosition(),
		InputSize(),
		2);
	ChangeNameInput = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("ChangeNameInput"));
	ChangeNameInput->SetText(FText::FromString(ChangeNameDraft));
	ChangeNameInput->SetHintText(
		NSLOCTEXT("RokModal", "ChangeNameInputHintSource", "点击此处输入城主昵称..."));
	FEditableTextBoxStyle InputStyle = FEditableTextBoxStyle::GetDefault();
	FSlateBrush NoInputBrush; NoInputBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	InputStyle
		.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), InputFontPoints))
		.SetForegroundColor(FLinearColor::White)
		.SetFocusedForegroundColor(FLinearColor::White)
		.SetBackgroundImageNormal(NoInputBrush)
		.SetBackgroundImageHovered(NoInputBrush)
		.SetBackgroundImageFocused(NoInputBrush)
		.SetBackgroundImageReadOnly(NoInputBrush)
		.SetPadding(FMargin(InputHorizontalPadding, 0));
	ChangeNameInput->SetWidgetStyle(InputStyle);
	ChangeNameInput->SetJustification(ETextJustify::Left);
	ChangeNameInput->SetSelectAllTextWhenFocused(true);
	ChangeNameInput->SetClearKeyboardFocusOnCommit(false);
	ChangeNameInput->OnTextChanged.AddDynamic(
		this,
		&URokUnityModalWidget::HandleChangeNameTextChanged);
	PlaceOnCanvas(
		ModeCanvas,
		ChangeNameInput,
		InputPosition(),
		InputSize(),
		4);

	ChangeNameCharacterCount = MakeModalText(
		TEXT("ChangeNameCharacterCount"),
		16,
		FLinearColor(0.76f, 0.72f, 0.64f, 1.0f),
		false,
		ETextJustify::Right);
	PlaceOnCanvas(
		ModeCanvas,
		ChangeNameCharacterCount,
		FVector2D(1017.0f, 333.0f),
		FVector2D(94.0f, 24.0f),
		4);

	UTextBlock* Description = MakeModalText(
		TEXT("ChangeNameDescription"),
		DescriptionFontPoints,
		FLinearColor::White,
		false,
		ETextJustify::Left);
	Description->SetText(NSLOCTEXT(
		"RokModal",
		"ChangeNameDescriptionSource",
		"您能够输入3至15个字符，可以由数字、字母和特殊符号组成"));
	Description->SetAutoWrapText(true);
	Description->SetShadowOffset(FVector2D::ZeroVector);
	Description->SetShadowColorAndOpacity(FLinearColor::Transparent);
	Description->SetClipping(EWidgetClipping::ClipToBounds);
	PlaceOnCanvas(
		ModeCanvas,
		Description,
		DescriptionPosition(),
		DescriptionSize(),
		3);

	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(
			TEXT("ChangeNamePermitIcon"),
			Item5AtlasPath,
			RenamePermitItemRect,
			Item5AtlasSize,
			FVector2D(48.0f)),
		FVector2D(389.0f, 483.0f),
		FVector2D(48.0f),
		3);
	UTextBlock* CostDescription = MakeModalText(
		TEXT("ChangeNameCostDescription"),
		13,
		FLinearColor(0.94f, 0.86f, 0.59f, 1.0f),
		true,
		ETextJustify::Left);
	CostDescription->SetText(FText::Format(
		NSLOCTEXT(
			"RokModal",
			"ChangeNameCostDescriptionText",
			"优先消耗改名许可 ×1；没有时消耗 {0} 宝石"),
		FText::AsNumber(ChangeNameGemCost)));
	CostDescription->SetAutoWrapText(true);
	PlaceOnCanvas(
		ModeCanvas,
		CostDescription,
		FVector2D(450.0f, 487.0f),
		FVector2D(620.0f, 42.0f),
		3);

	ChangeNameValidationFeedback = MakeModalText(
		TEXT("ChangeNameValidationFeedback"),
		12,
		FLinearColor(0.96f, 0.72f, 0.34f, 1.0f),
		true,
		ETextJustify::Center);
	ChangeNameValidationFeedback->SetAutoWrapText(true);
	PlaceOnCanvas(
		ModeCanvas,
		ChangeNameValidationFeedback,
		FVector2D(329.0f, 333.0f),
		FVector2D(670.0f, 32.0f),
		4);

	ChangeNameServiceBoundaryState = MakeModalText(
		TEXT("ChangeNameServiceBoundaryState"),
		11,
		FLinearColor(0.86f, 0.36f, 0.23f, 1.0f),
		true,
		ETextJustify::Center);
	ChangeNameServiceBoundaryState->SetText(NSLOCTEXT(
		"RokModal",
		"ChangeNameServiceBoundary",
		"服务器改名、权威背包与扣费状态尚未接入；不会修改本地资料。"));
	ChangeNameServiceBoundaryState->SetAutoWrapText(true);
	PlaceOnCanvas(
		ModeCanvas,
		ChangeNameServiceBoundaryState,
		FVector2D(341.0f, 539.0f),
		FVector2D(758.0f, 24.0f),
		4);

	ChangeNameConfirmButton = MakeTextAtlasButton(
		TEXT("ChangeNameConfirmButton"),
		CommonAtlasPath,
		ResourceGoldButtonRect,
		CommonAtlasSize,
		FVector2D(269.0f, 78.0f),
		NSLOCTEXT("RokModal", "ChangeNameConfirm", "确定"),
		25,
		FLinearColor::White,
		NSLOCTEXT("RokModal", "ChangeNameConfirmUnavailable", "服务未接入"));
	ChangeNameConfirmButton->OnClicked.AddDynamic(
		this,
		&URokUnityModalWidget::HandleChangeNameConfirmClicked);
	PlaceOnCanvas(
		ModeCanvas,
		ChangeNameConfirmButton,
		FVector2D(585.5f, 567.0f),
		FVector2D(269.0f, 78.0f),
		4);

	UpdateChangeNameValidationState();
}

void URokUnityModalWidget::ResetChangeNameWidgetPointers()
{
	ChangeNameInput = nullptr;
	ChangeNameCharacterCount = nullptr;
	ChangeNameValidationFeedback = nullptr;
	ChangeNameServiceBoundaryState = nullptr;
	ChangeNameConfirmButton = nullptr;
	bUpdatingChangeNameInput = false;
}

bool URokUnityModalWidget::EnsureChangeNameBannedWordsLoaded()
{
	if (bChangeNameBannedWordsLoadAttempted)
	{
		return bChangeNameBannedWordsLoaded;
	}

	bChangeNameBannedWordsLoadAttempted = true;
	bChangeNameBannedWordsLoaded = false;
	ChangeNameBannedWords.Reset();
	const FString BlockedWordsPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir(),
		ChangeNameBlockedWordsRelativePath);
	TArray<uint8> BinaryData;
	if (!FFileHelper::LoadFileToArray(BinaryData, *BlockedWordsPath))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok change-name UI could not load Unity blocked words: %s"),
			*BlockedWordsPath);
		return false;
	}

	FMemoryReader Reader(BinaryData, true);
	int32 EntryCount = 0;
	Reader << EntryCount;
	if (Reader.IsError() || EntryCount <= 0 || EntryCount > 10000)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok change-name UI rejected invalid Unity blocked-word count: %d"),
			EntryCount);
		return false;
	}

	ChangeNameBannedWords.Reserve(EntryCount);
	for (int32 Index = 0; Index < EntryCount; ++Index)
	{
		uint16 Utf8ByteLength = 0;
		Reader << Utf8ByteLength;
		if (Reader.IsError()
			|| Reader.Tell() + static_cast<int64>(Utf8ByteLength)
				> Reader.TotalSize())
		{
			ChangeNameBannedWords.Reset();
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Rok change-name UI found a truncated Unity blocked-word entry: index=%d"),
				Index);
			return false;
		}

		TArray<uint8> Utf8Bytes;
		Utf8Bytes.SetNumUninitialized(Utf8ByteLength);
		if (Utf8ByteLength > 0)
		{
			Reader.Serialize(Utf8Bytes.GetData(), Utf8ByteLength);
		}
		if (Reader.IsError())
		{
			ChangeNameBannedWords.Reset();
			return false;
		}

		if (Utf8ByteLength == 0)
		{
			ChangeNameBannedWords.Emplace();
			continue;
		}
		// Unity BannedWord.ChackHasBannedWord intentionally starts at index 1.
		// Preserve the first binary row as an empty sentinel so this runtime
		// matches that lookup order while still validating all 208 source rows.
		if (Index == 0)
		{
			ChangeNameBannedWords.Emplace();
			continue;
		}
		const FUTF8ToTCHAR Converted(
			reinterpret_cast<const ANSICHAR*>(Utf8Bytes.GetData()),
			Utf8Bytes.Num());
		ChangeNameBannedWords.Emplace(Converted.Length(), Converted.Get());
	}

	bChangeNameBannedWordsLoaded =
		ChangeNameBannedWords.Num() == EntryCount;
	if (bChangeNameBannedWordsLoaded)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Rok change-name UI loaded Unity blocked words: count=%d source=%s"),
			ChangeNameBannedWords.Num(),
			*BlockedWordsPath);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok change-name UI did not load the complete Unity blocked-word table: count=%d source=%s"),
			ChangeNameBannedWords.Num(),
			*BlockedWordsPath);
	}
	return bChangeNameBannedWordsLoaded;
}

FText URokUnityModalWidget::GetChangeNameValidationMessage(
	ERokChangeNameValidationResult Result) const
{
	switch (Result)
	{
	case ERokChangeNameValidationResult::Empty:
		return NSLOCTEXT("RokModal", "ChangeNameEmpty", "请输入新的城主名称。");
	case ERokChangeNameValidationResult::TooShort:
		return FText::Format(
			NSLOCTEXT("RokModal", "ChangeNameTooShort", "名称至少需要 {0} 个字符。"),
			FText::AsNumber(ChangeNameMinLength));
	case ERokChangeNameValidationResult::TooLong:
		return FText::Format(
			NSLOCTEXT("RokModal", "ChangeNameTooLong", "名称最多允许 {0} 个字符。"),
			FText::AsNumber(ChangeNameMaxLength));
	case ERokChangeNameValidationResult::OnlyDigits:
		return NSLOCTEXT("RokModal", "ChangeNameOnlyDigits", "名称不能只由数字组成。");
	case ERokChangeNameValidationResult::SameAsCurrent:
		return NSLOCTEXT("RokModal", "ChangeNameSame", "新名称不能与当前名称相同。");
	case ERokChangeNameValidationResult::BannedWord:
		return NSLOCTEXT("RokModal", "ChangeNameBanned", "名称中包含不可使用的词语。");
	case ERokChangeNameValidationResult::BannedWordDataUnavailable:
		return NSLOCTEXT(
			"RokModal",
			"ChangeNameBannedDataUnavailable",
			"Unity 禁词表未加载，已安全禁止提交。");
	case ERokChangeNameValidationResult::Valid:
	default:
		return NSLOCTEXT(
			"RokModal",
			"ChangeNameLocallyValid",
			"本地格式校验通过；服务器仍需复核重名、禁词和消耗。");
	}
}

void URokUnityModalWidget::UpdateChangeNameValidationState()
{
	const ERokChangeNameValidationResult Result = ValidateChangeNameCandidate(
		ChangeNameDraft,
		ChangeNameCurrentName,
		bChangeNameHasCurrentName,
		ChangeNameBannedWords,
		bChangeNameBannedWordsLoaded);
	if (ChangeNameCharacterCount)
	{
		ChangeNameCharacterCount->SetText(FText::Format(
			NSLOCTEXT("RokModal", "ChangeNameCharacterCountFormat", "{0}/{1}"),
			FText::AsNumber(ChangeNameDraft.Len()),
			FText::AsNumber(ChangeNameMaxLength)));
	}
	if (ChangeNameValidationFeedback)
	{
		ChangeNameValidationFeedback->SetText(
			GetChangeNameValidationMessage(Result));
		ChangeNameValidationFeedback->SetColorAndOpacity(FSlateColor(
			Result == ERokChangeNameValidationResult::Valid
				? FLinearColor(0.43f, 0.84f, 0.45f, 1.0f)
				: FLinearColor(0.96f, 0.58f, 0.26f, 1.0f)));
	}
	if (ChangeNameConfirmButton)
	{
		ChangeNameConfirmButton->SetIsEnabled(
			Result == ERokChangeNameValidationResult::Valid
			&& bChangeNameHasCurrentName
			&& bChangeNameSubmitServiceAvailable);
	}
}

void URokUnityModalWidget::BuildResourceInventory()
{
	UCanvasPanel* Viewport = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResourceListViewport"));
	Viewport->SetClipping(EWidgetClipping::ClipToBounds);
	PlaceOnCanvas(ModeCanvas, Viewport, FVector2D(207.0f, 168.3f), FVector2D(1026.0f, 609.4f), 1);

	static const int32 Amounts[4][4] = {
		{10000, 50000, 150000, 1000},
		{10000, 50000, 150000, 1000},
		{7500, 37500, 112500, 750},
		{3000, 15000, 50000, 500},
	};
	static const int32 Qualities[4] = {2, 2, 3, 1};
	static const int32 OwnedCounts[4] = {3, 1, 1, 0};
	static const FText ResourceNames[4] = {
		NSLOCTEXT("RokModal", "Food", "食物"),
		NSLOCTEXT("RokModal", "Wood", "木材"),
		NSLOCTEXT("RokModal", "Stone", "石料"),
		NSLOCTEXT("RokModal", "Gold", "金币"),
	};

	int32 RowCount = 0;
	if (bReferenceResourceCaptureState)
	{
		for (int32 RowIndex = 0; RowIndex < 4; ++RowIndex)
		{
			UCanvasPanel* Row = MakeResourceRow(
				RowIndex,
				Amounts[SelectedResourceCategory][RowIndex],
				Qualities[RowIndex],
				OwnedCounts[RowIndex],
				RowIndex == 3 ? 5 : 0,
				ResourceNames[SelectedResourceCategory],
				ItemIconRects[SelectedResourceCategory]);
			PlaceOnCanvas(Viewport, Row, FVector2D(19.0f, RowIndex * 156.0f), FVector2D(988.0f, 156.0f));
			++RowCount;
		}
		// Owned counts come from the server item inventory; reference rows must
		// stay explicitly labeled instead of reading as account state.
		UTextBlock* ResourceReferenceTag = MakeModalText(
			TEXT("ResourceInventoryReferenceTag"),
			14,
			FLinearColor(0.62f, 0.62f, 0.58f, 1.0f),
			false,
			ETextJustify::Right);
		ResourceReferenceTag->SetText(NSLOCTEXT(
			"RokModal",
			"ResourceInventoryReferenceTag",
			"资源道具 · 本地参考态（非账号状态）"));
		PlaceOnCanvas(ModeCanvas, ResourceReferenceTag, FVector2D(830.0f, 745.0f), FVector2D(400.0f, 24.0f), 6);
	}
	else
	{
		// Production has no server item inventory yet; keep the list honest
		// instead of presenting fabricated owned counts.
		UTextBlock* EmptyState = MakeModalText(
			TEXT("ResourceInventoryEmptyState"),
			22,
			FLinearColor(0.55f, 0.55f, 0.51f, 1.0f),
			false,
			ETextJustify::Center);
		EmptyState->SetText(NSLOCTEXT(
			"RokModal",
			"ResourceInventoryEmptyState",
			"当前暂无可用的资源道具"));
		PlaceOnCanvas(ModeCanvas, EmptyState, FVector2D(207.0f, 400.0f), FVector2D(1026.0f, 40.0f), 2);
		UTextBlock* ServiceState = MakeModalText(
			TEXT("ResourceInventoryServiceState"),
			17,
			FLinearColor(0.62f, 0.62f, 0.58f, 1.0f),
			false,
			ETextJustify::Center);
		ServiceState->SetText(NSLOCTEXT(
			"RokModal",
			"ResourceInventoryServiceState",
			"道具背包需要服务器数据，连接服务器后更新。"));
		PlaceOnCanvas(ModeCanvas, ServiceState, FVector2D(207.0f, 452.0f), FVector2D(1026.0f, 32.0f), 2);
	}

	for (int32 CategoryIndex = 0; CategoryIndex < 4; ++CategoryIndex)
	{
		UButton* Tab = MakeResourceTab(CategoryIndex, CategoryIndex == SelectedResourceCategory);
		switch (CategoryIndex)
		{
		case 0: Tab->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleResourceTabFood); break;
		case 1: Tab->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleResourceTabWood); break;
		case 2: Tab->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleResourceTabStone); break;
		case 3: Tab->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleResourceTabGold); break;
		default: break;
		}
		PlaceOnCanvas(ModeCanvas, Tab, FVector2D(1231.0f, 214.0f + CategoryIndex * 109.0f), FVector2D(144.0f, 99.0f), 4);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok resource inventory: category=%d reference=%d rows=%d authority=%s."),
		SelectedResourceCategory,
		bReferenceResourceCaptureState ? 1 : 0,
		RowCount,
		bReferenceResourceCaptureState ? TEXT("local_fixture") : TEXT("server"));
}

void URokUnityModalWidget::BuildWarWarning()
{
	constexpr float ContentX = 230.0f;
	constexpr float ContentWidth = 980.0f;

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok war warning screen: reference=%d authority=%s."),
		bReferenceWarWarningCaptureState ? 1 : 0,
		bReferenceWarWarningCaptureState ? TEXT("local_fixture") : TEXT("server"));

	if (!bReferenceWarWarningCaptureState)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasBorder(
				TEXT("WarWarningServiceBoundaryPanel"),
				CommonAtlasPath,
				CommonPanelRect,
				CommonAtlasSize,
				FVector2D(ContentWidth, 250.0f),
				CommonPanelMargin),
			FVector2D(ContentX, 245.0f),
			FVector2D(ContentWidth, 250.0f),
			1);

		UTextBlock* BoundaryTitle = MakeModalText(
			TEXT("WarWarningServiceBoundaryTitle"),
			28,
			FLinearColor(0.30f, 0.22f, 0.12f, 1.0f),
			true,
			ETextJustify::Center);
		BoundaryTitle->SetText(NSLOCTEXT("RokModal", "WarWarningUnavailableTitle", "预警服务尚未接入"));
		PlaceOnCanvas(ModeCanvas, BoundaryTitle, FVector2D(ContentX + 30.0f, 280.0f), FVector2D(ContentWidth - 60.0f, 40.0f), 2);

		UTextBlock* BoundaryState = MakeModalText(
			TEXT("WarWarningServiceBoundaryState"),
			21,
			FLinearColor(0.38f, 0.31f, 0.22f, 1.0f),
			false,
			ETextJustify::Center);
		BoundaryState->SetAutoWrapText(true);
		BoundaryState->SetText(NSLOCTEXT(
			"RokModal",
			"WarWarningServiceBoundary",
			"当前客户端还没有接入服务器 Role_EarlyWarningInfo 数据，因此无法验证侦察、攻击、增援或运输预警。"));
		PlaceOnCanvas(ModeCanvas, BoundaryState, FVector2D(ContentX + 85.0f, 338.0f), FVector2D(ContentWidth - 170.0f, 66.0f), 2);

		UTextBlock* BoundaryActions = MakeModalText(
			TEXT("WarWarningServerActionsDisabled"),
			18,
			FLinearColor(0.61f, 0.38f, 0.14f, 1.0f),
			true,
			ETextJustify::Center);
		BoundaryActions->SetAutoWrapText(true);
		BoundaryActions->SetText(NSLOCTEXT(
			"RokModal",
			"WarWarningActionsBoundary",
			"忽略、定位和详情操作将在服务器提供预警索引、抵达时间与目标位置后启用。"));
		PlaceOnCanvas(ModeCanvas, BoundaryActions, FVector2D(ContentX + 85.0f, 419.0f), FVector2D(ContentWidth - 170.0f, 52.0f), 2);
		return;
	}

	UTextBlock* ReferenceNotice = MakeModalText(
		TEXT("WarWarningReferencePreviewNotice"),
		20,
		FLinearColor(1.0f, 0.78f, 0.25f, 1.0f),
		true,
		ETextJustify::Center);
	ReferenceNotice->SetText(NSLOCTEXT(
		"RokModal",
		"WarWarningReferenceNotice",
		"Unity 参考预览/非实时数据：以下内容只用于 UI 结构验收，不代表玩家当前预警"));
	PlaceOnCanvas(ModeCanvas, ReferenceNotice, FVector2D(ContentX, 176.0f), FVector2D(ContentWidth, 36.0f), 4);

	UTextBlock* IgnorePreview = MakeModalText(
		TEXT("WarWarningReferenceIgnoreAllDisabled"),
		17,
		FLinearColor(0.63f, 0.38f, 0.16f, 1.0f),
		true,
		ETextJustify::Right);
	IgnorePreview->SetText(NSLOCTEXT("RokModal", "WarWarningIgnoreAllPreview", "全部忽略（参考预览中禁用）"));
	PlaceOnCanvas(ModeCanvas, IgnorePreview, FVector2D(880.0f, 218.0f), FVector2D(330.0f, 30.0f), 3);

	static const TCHAR* TypeShortLabels[] = {TEXT("侦"), TEXT("战"), TEXT("援"), TEXT("运")};
	static const FText TypeTitles[] = {
		NSLOCTEXT("RokModal", "WarningScout", "侦察预警 · 参考"),
		NSLOCTEXT("RokModal", "WarningAttack", "攻击预警 · 参考"),
		NSLOCTEXT("RokModal", "WarningReinforce", "增援预警 · 参考"),
		NSLOCTEXT("RokModal", "WarningTransport", "运输抵达 · 参考"),
	};
	static const FText TypeDescriptions[] = {
		NSLOCTEXT("RokModal", "WarningScoutDescription", "[ABC] 斥候正在侦察你的城市"),
		NSLOCTEXT("RokModal", "WarningAttackDescription", "[RED] 集结部队 · 兵力 125,000"),
		NSLOCTEXT("RokModal", "WarningReinforceDescription", "[ALLY] 友军增援 · 兵力 48,000"),
		NSLOCTEXT("RokModal", "WarningTransportDescription", "[ALLY] 运输队 · 资源 320,000"),
	};
	static const FText TypeTargets[] = {
		NSLOCTEXT("RokModal", "WarningScoutTarget", "目标：城市 · 00:08:42"),
		NSLOCTEXT("RokModal", "WarningAttackTarget", "目标：城市 · 00:12:18"),
		NSLOCTEXT("RokModal", "WarningReinforceTarget", "目标：城市 · 00:05:33"),
		NSLOCTEXT("RokModal", "WarningTransportTarget", "目标：城市 · 00:03:20"),
	};
	static const FLinearColor TypeColors[] = {
		FLinearColor(0.18f, 0.57f, 0.82f, 1.0f),
		FLinearColor(0.76f, 0.17f, 0.12f, 1.0f),
		FLinearColor(0.20f, 0.64f, 0.33f, 1.0f),
		FLinearColor(0.74f, 0.48f, 0.12f, 1.0f),
	};
	static const FName RowNames[] = {
		FName(TEXT("WarWarningRow_0")),
		FName(TEXT("WarWarningRow_1")),
		FName(TEXT("WarWarningRow_2")),
		FName(TEXT("WarWarningRow_3")),
	};

	for (int32 RowIndex = 0; RowIndex < 4; ++RowIndex)
	{
		UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			RowNames[RowIndex]);
		PlaceOnCanvas(
			Row,
			MakeAtlasBorder(
				*FString::Printf(TEXT("WarWarningRowBackground_%d"), RowIndex),
				CommonAtlasPath,
				ResourceRowRect,
				CommonAtlasSize,
				FVector2D(ContentWidth, 72.0f),
				ResourceRowMargin),
			FVector2D::ZeroVector,
			FVector2D(ContentWidth, 72.0f));

		UBorder* TypeBadge = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("WarWarningTypeBadge_%d"), RowIndex));
		TypeBadge->SetBrushColor(TypeColors[RowIndex]);
		TypeBadge->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(Row, TypeBadge, FVector2D(14.0f, 11.0f), FVector2D(50.0f, 50.0f), 1);

		UTextBlock* TypeShort = MakeModalText(
			*FString::Printf(TEXT("WarWarningTypeShort_%d"), RowIndex),
			22,
			FLinearColor::White,
			true,
			ETextJustify::Center);
		TypeShort->SetText(FText::FromString(FString(TypeShortLabels[RowIndex])));
		PlaceOnCanvas(Row, TypeShort, FVector2D(14.0f, 20.0f), FVector2D(50.0f, 32.0f), 2);

		UTextBlock* Title = MakeModalText(
			*FString::Printf(TEXT("WarWarningTypeTitle_%d"), RowIndex),
			20,
			FLinearColor(0.28f, 0.22f, 0.14f, 1.0f),
			true);
		Title->SetText(TypeTitles[RowIndex]);
		PlaceOnCanvas(Row, Title, FVector2D(82.0f, 7.0f), FVector2D(340.0f, 28.0f), 2);

		UTextBlock* Description = MakeModalText(
			*FString::Printf(TEXT("WarWarningDescription_%d"), RowIndex),
			17,
			FLinearColor(0.38f, 0.31f, 0.22f, 1.0f));
		Description->SetText(TypeDescriptions[RowIndex]);
		PlaceOnCanvas(Row, Description, FVector2D(82.0f, 37.0f), FVector2D(510.0f, 26.0f), 2);

		UTextBlock* Target = MakeModalText(
			*FString::Printf(TEXT("WarWarningTarget_%d"), RowIndex),
			17,
			FLinearColor(0.48f, 0.28f, 0.10f, 1.0f),
			true,
			ETextJustify::Right);
		Target->SetText(TypeTargets[RowIndex]);
		PlaceOnCanvas(Row, Target, FVector2D(600.0f, 23.0f), FVector2D(350.0f, 28.0f), 2);

		PlaceOnCanvas(
			ModeCanvas,
			Row,
			FVector2D(ContentX, 252.0f + RowIndex * 80.0f),
			FVector2D(ContentWidth, 72.0f),
			2);
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(
			TEXT("WarWarningDetailStructure"),
			CommonAtlasPath,
			CommonPanelRect,
			CommonAtlasSize,
			FVector2D(ContentWidth, 142.0f),
			CommonPanelMargin),
		FVector2D(ContentX, 576.0f),
		FVector2D(ContentWidth, 142.0f),
		1);

	UTextBlock* DetailTitle = MakeModalText(
		TEXT("WarWarningDetailTitle"),
		20,
		FLinearColor(0.68f, 0.18f, 0.12f, 1.0f),
		true);
	DetailTitle->SetText(NSLOCTEXT("RokModal", "WarWarningDetailReferenceTitle", "攻击详情（Unity 参考结构 / 非实时数据）"));
	PlaceOnCanvas(ModeCanvas, DetailTitle, FVector2D(ContentX + 24.0f, 588.0f), FVector2D(520.0f, 30.0f), 2);

	UTextBlock* DetailBody = MakeModalText(
		TEXT("WarWarningDetailBody"),
		17,
		FLinearColor(0.35f, 0.29f, 0.21f, 1.0f));
	DetailBody->SetAutoWrapText(true);
	DetailBody->SetText(NSLOCTEXT(
		"RokModal",
		"WarWarningDetailReferenceBody",
		"主将：参考统帅 A　副将：参考统帅 B\n兵种构成：步兵 80,000 / 弓兵 45,000　目标：城市坐标（参考）"));
	PlaceOnCanvas(ModeCanvas, DetailBody, FVector2D(ContentX + 24.0f, 624.0f), FVector2D(ContentWidth - 48.0f, 72.0f), 2);
}

void URokUnityModalWidget::BuildBuffList()
{
	constexpr float PopupX = 0.0f;
	constexpr float PopupY = 0.0f;
	constexpr float PopupWidth = 500.0f;
	constexpr float PopupHeight = 400.0f;
	constexpr float RowX = PopupX + 42.0f;
	constexpr float RowWidth = 416.0f;

	// UI_Pop_BuffList.prefab: pl_pos parents img_bg and pl_rect. The full-screen
	// dismiss mask is outside this group and must never scale with the popup.
	BuffListAnimationRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("BuffListAnimationRoot"));
	PlaceOnCanvas(ModeCanvas, BuffListAnimationRoot, FVector2D(470.0f, 205.0f),
		FVector2D(PopupWidth, PopupHeight));
	// Source img_bg width 450.3, pivot X .9, offset X 3.3 from pl_pos.
	// Preserve its relative origin within the existing reconstructed panel.
	BuffListAnimationRoot->SetRenderTransformPivot(FVector2D(0.9f - 3.3f / 450.3f, 0.0f));

	PlaceOnCanvas(
		BuffListAnimationRoot,
		MakeAtlasBorder(
			TEXT("BuffListPopupBody"),
			FrameAtlasPath,
			WindowBodyRect,
			FrameAtlasSize,
			FVector2D(PopupWidth, PopupHeight),
			WindowBodyMargin),
		FVector2D(PopupX, PopupY),
		FVector2D(PopupWidth, PopupHeight),
		1);
	PlaceOnCanvas(
		BuffListAnimationRoot,
		MakeAtlasBorder(
			TEXT("BuffListPopupHeader"),
			FrameAtlasPath,
			WindowHeaderRect,
			FrameAtlasSize,
			FVector2D(PopupWidth, 60.0f),
			WindowHeaderMargin),
		FVector2D(PopupX, PopupY),
		FVector2D(PopupWidth, 60.0f),
		2);

	UTextBlock* PopupTitle = MakeModalText(
		TEXT("BuffListPopupTitle"),
		25,
		FLinearColor(0.96f, 0.94f, 0.88f, 1.0f),
		true,
		ETextJustify::Center);
	PopupTitle->SetText(NSLOCTEXT("RokModal", "BuffListTitle", "城市增益"));
	PlaceOnCanvas(BuffListAnimationRoot, PopupTitle, FVector2D(PopupX + 30.0f, PopupY + 9.0f), FVector2D(PopupWidth - 60.0f, 38.0f), 3);

	if (!bReferenceBuffListCaptureState)
	{
		PlaceOnCanvas(
			BuffListAnimationRoot,
			MakeAtlasBorder(
				TEXT("BuffListServiceBoundaryPanel"),
				CommonAtlasPath,
				CommonPanelRect,
				CommonAtlasSize,
				FVector2D(456.0f, 205.0f),
				CommonPanelMargin),
			FVector2D(PopupX + 22.0f, PopupY + 80.0f),
			FVector2D(456.0f, 205.0f),
			2);

		UTextBlock* BoundaryTitle = MakeModalText(
			TEXT("BuffListServiceBoundaryTitle"),
			21,
			FLinearColor(0.30f, 0.22f, 0.12f, 1.0f),
			true,
			ETextJustify::Center);
		BoundaryTitle->SetText(NSLOCTEXT("RokModal", "BuffListUnavailableTitle", "城市增益状态尚未接入"));
		PlaceOnCanvas(BuffListAnimationRoot, BoundaryTitle, FVector2D(PopupX + 42.0f, PopupY + 98.0f), FVector2D(416.0f, 32.0f), 3);

		UTextBlock* BoundaryState = MakeModalText(
			TEXT("BuffListServiceBoundaryState"),
			17,
			FLinearColor(0.38f, 0.31f, 0.22f, 1.0f),
			false,
			ETextJustify::Center);
		BoundaryState->SetAutoWrapText(true);
		BoundaryState->SetText(NSLOCTEXT(
			"RokModal",
			"BuffListServiceBoundary",
			"激活状态、效果数值和剩余时间需要服务器 CityBuff 状态；当前不显示伪造数据。"));
		PlaceOnCanvas(BuffListAnimationRoot, BoundaryState, FVector2D(PopupX + 52.0f, PopupY + 143.0f), FVector2D(396.0f, 66.0f), 3);

		UTextBlock* NavigationHint = MakeModalText(
			TEXT("BuffListCityManagerNavigationHint"),
			15,
			FLinearColor(0.61f, 0.38f, 0.14f, 1.0f),
			true,
			ETextJustify::Center);
		NavigationHint->SetAutoWrapText(true);
		NavigationHint->SetText(NSLOCTEXT("RokModal", "BuffListMoreHint", "“更多”只进入城市管理，不代表已激活任何增益。"));
		PlaceOnCanvas(BuffListAnimationRoot, NavigationHint, FVector2D(PopupX + 52.0f, PopupY + 225.0f), FVector2D(396.0f, 38.0f), 3);
	}
	else
	{
		UTextBlock* ReferenceNotice = MakeModalText(
			TEXT("BuffListReferencePreviewNotice"),
			15,
			FLinearColor(1.0f, 0.78f, 0.25f, 1.0f),
			true,
			ETextJustify::Center);
		ReferenceNotice->SetText(NSLOCTEXT(
			"RokModal",
			"BuffListReferenceNotice",
			"城市增益静态配置预览 · 非实时/未激活"));
		PlaceOnCanvas(BuffListAnimationRoot, ReferenceNotice, FVector2D(PopupX + 22.0f, PopupY + 64.0f), FVector2D(456.0f, 28.0f), 4);

		FString DefinitionJson;
		const FString DefinitionPath = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("RokUnityMigration"), TEXT("city_buff_reference.json"));
		TSharedPtr<FJsonObject> DefinitionRoot;
		const bool bManifestLoaded = FFileHelper::LoadFileToString(DefinitionJson, *DefinitionPath)
			&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(DefinitionJson), DefinitionRoot)
			&& DefinitionRoot.IsValid();
		const TArray<TSharedPtr<FJsonValue>>* Definitions = nullptr;
		FString DefinitionSchema;
		const bool bDefinitionsValid = bManifestLoaded
			&& DefinitionRoot->TryGetStringField(TEXT("schema"), DefinitionSchema)
			&& DefinitionSchema == TEXT("rok.city_buff_reference.v2")
			&& DefinitionRoot->TryGetArrayField(TEXT("definitions"), Definitions)
			&& Definitions != nullptr;
		int32 ShownCount = 0;
		if (bDefinitionsValid)
		{
			for (const TSharedPtr<FJsonValue>& DefinitionValue : *Definitions)
			{
				if (ShownCount >= 2) break;
				const TSharedPtr<FJsonObject> Definition = DefinitionValue.IsValid() ? DefinitionValue->AsObject() : nullptr;
				if (!Definition.IsValid()) continue;
				double ConfigDuration = 0.0;
				FString TitleCn;
				FString IconAtlas;
				const TArray<TSharedPtr<FJsonValue>>* IconRectValues = nullptr;
				if (!Definition->TryGetNumberField(TEXT("config_duration_seconds"), ConfigDuration)
					|| !Definition->TryGetStringField(TEXT("title_cn"), TitleCn)
					|| !Definition->TryGetStringField(TEXT("icon_atlas"), IconAtlas)
					|| !Definition->TryGetArrayField(TEXT("icon_rect"), IconRectValues)
					|| ConfigDuration < 0 || IconAtlas != MainMenuAtlasPath
					|| !IconRectValues || IconRectValues->Num() != 4) continue;
				double IconNumbers[4] = {};
				bool bIconRectValid = true;
				for (int32 Component = 0; Component < 4; ++Component)
				{
					bIconRectValid &= (*IconRectValues)[Component].IsValid()
						&& (*IconRectValues)[Component]->TryGetNumber(IconNumbers[Component]);
				}
				if (!bIconRectValid || IconNumbers[0] < 0 || IconNumbers[1] < 0
					|| IconNumbers[2] <= 0 || IconNumbers[3] <= 0
					|| IconNumbers[0] + IconNumbers[2] > MainMenuAtlasSize.X
					|| IconNumbers[1] + IconNumbers[3] > MainMenuAtlasSize.Y) continue;
				const int32 RowIndex = ShownCount++;
				UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(
					UCanvasPanel::StaticClass(), *FString::Printf(TEXT("BuffListDefinitionRow_%d"), RowIndex));
				PlaceOnCanvas(Row, MakeAtlasBorder(
					*FString::Printf(TEXT("BuffListRowBackground_%d"), RowIndex),
					CommonAtlasPath, ResourceRowRect, CommonAtlasSize,
					FVector2D(RowWidth, 80.0f), ResourceRowMargin),
					FVector2D::ZeroVector, FVector2D(RowWidth, 80.0f));

				PlaceOnCanvas(Row, MakeAtlasImage(
					*FString::Printf(TEXT("BuffListDefinitionIcon_%d"), RowIndex),
					MainMenuAtlasPath,
					SpriteRect(static_cast<int32>(IconNumbers[0]), static_cast<int32>(IconNumbers[1]),
						static_cast<int32>(IconNumbers[2]), static_cast<int32>(IconNumbers[3])),
					MainMenuAtlasSize, FVector2D(54.0f, 54.0f)),
					FVector2D(13.0f, 13.0f), FVector2D(54.0f, 54.0f), 1);

				UTextBlock* Title = MakeModalText(
					*FString::Printf(TEXT("BuffListDefinitionTitle_%d"), RowIndex), 17,
					FLinearColor(0.28f, 0.22f, 0.14f, 1.0f), true);
				Title->SetText(FText::FromString(TitleCn.IsEmpty() ? FString(TEXT("名称本地化不可用")) : TitleCn));
				PlaceOnCanvas(Row, Title, FVector2D(82.0f, 9.0f), FVector2D(310.0f, 27.0f), 2);

				UTextBlock* StaticFields = MakeModalText(
					*FString::Printf(TEXT("BuffListDefinitionFields_%d"), RowIndex), 13,
					FLinearColor(0.50f, 0.43f, 0.34f, 1.0f));
				StaticFields->SetText(FText::FromString(FString::Printf(
					TEXT("配置时长 %d 秒（非剩余时间）"), static_cast<int32>(ConfigDuration))));
				PlaceOnCanvas(Row, StaticFields, FVector2D(82.0f, 39.0f), FVector2D(310.0f, 23.0f), 2);
				PlaceOnCanvas(BuffListAnimationRoot, Row,
					FVector2D(RowX, PopupY + 102.0f + RowIndex * 86.0f), FVector2D(RowWidth, 80.0f), 3);
			}
		}
		if (ShownCount == 0)
		{
			UTextBlock* MissingDefinitions = MakeModalText(TEXT("BuffListDefinitionUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			MissingDefinitions->SetText(NSLOCTEXT("RokModal", "BuffListDefinitionUnavailable", "增益静态配置未发布或不可读取"));
			PlaceOnCanvas(BuffListAnimationRoot, MissingDefinitions,
				FVector2D(PopupX + 42.0f, PopupY + 132.0f), FVector2D(416.0f, 60.0f), 3);
		}
	}

	UTextBlock* TouchCloseHint = MakeModalText(
		TEXT("BuffListTouchCloseHint"),
		13,
		FLinearColor(0.50f, 0.43f, 0.34f, 1.0f));
	TouchCloseHint->SetText(NSLOCTEXT("RokModal", "BuffListTouchClose", "点击弹窗外区域关闭"));
	PlaceOnCanvas(BuffListAnimationRoot, TouchCloseHint, FVector2D(PopupX + 24.0f, PopupY + 350.0f), FVector2D(220.0f, 24.0f), 3);

	UButton* MoreButton = MakeTextAtlasButton(
		TEXT("BuffListMoreButton"),
		CommonAtlasPath,
		ResourceBlueButtonRect,
		CommonAtlasSize,
		FVector2D(160.0f, 52.0f),
		NSLOCTEXT("RokModal", "BuffListMore", "更多"),
		20,
		FLinearColor::White);
	MoreButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleBuffListMoreClicked);
	PlaceOnCanvas(BuffListAnimationRoot, MoreButton, FVector2D(PopupX + 316.0f, PopupY + 330.0f), FVector2D(160.0f, 52.0f), 4);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok buff list screen: reference=%d authority=%s."),
		bReferenceBuffListCaptureState ? 1 : 0,
		bReferenceBuffListCaptureState ? TEXT("local_fixture") : TEXT("server"));
}

UCanvasPanel* URokUnityModalWidget::MakeResourceRow(
	int32 RowIndex,
	int32 Amount,
	int32 Quality,
	int32 OwnedCount,
	int32 ShortcutPrice,
	const FText& ResourceName,
	const FIntRect& ItemIconRect)
{
	UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("ResourceRow_%d"), RowIndex));
	PlaceOnCanvas(Row, MakeAtlasBorder(*FString::Printf(TEXT("ResourceRowBackground_%d"), RowIndex), CommonAtlasPath, ResourceRowRect, CommonAtlasSize, FVector2D(988.0f, 145.0f), ResourceRowMargin), FVector2D(0.0f, 14.5f), FVector2D(988.0f, 145.0f));
	const int32 QualityIndex = FMath::Clamp(Quality - 1, 0, 2);
	PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("ResourceQuality_%d"), RowIndex), Item0AtlasPath, ItemQualityRects[QualityIndex], Item0AtlasSize, FVector2D(134.0f)), FVector2D(10.0f, 20.0f), FVector2D(134.0f), 1);
	PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("ResourceItemIcon_%d"), RowIndex), Item1AtlasPath, ItemIconRect, Item1AtlasSize, FVector2D(120.0f)), FVector2D(17.0f, 27.0f), FVector2D(120.0f), 2);
	PlaceOnCanvas(Row, MakeAtlasBorder(*FString::Printf(TEXT("ResourceAmountStrip_%d"), RowIndex), CommonAtlasPath, ItemAmountStripRect, CommonAtlasSize, FVector2D(120.0f, 26.12f), FMargin(43.0f / 120.0f, 0.0f, 41.0f / 120.0f, 0.0f)), FVector2D(17.0f, 25.4f), FVector2D(120.0f, 26.12f), 3);
	UTextBlock* AmountBadge = MakeModalText(*FString::Printf(TEXT("ResourceAmountBadge_%d"), RowIndex), 15, FLinearColor::White, true, ETextJustify::Center);
	AmountBadge->SetText(FormatNumber(Amount));
	PlaceOnCanvas(Row, AmountBadge, FVector2D(17.0f, 24.0f), FVector2D(120.0f, 27.0f), 4);

	UTextBlock* NameText = MakeModalText(*FString::Printf(TEXT("ResourceName_%d"), RowIndex), 24, FLinearColor(0.28f, 0.22f, 0.14f, 1.0f), true);
	NameText->SetText(FText::Format(NSLOCTEXT("RokModal", "ResourceItemName", "{0} {1}"), FormatNumber(Amount), ResourceName));
	PlaceOnCanvas(Row, NameText, FVector2D(158.4f, 35.2f), FVector2D(590.0f, 27.2f), 2);
	UTextBlock* Description = MakeModalText(*FString::Printf(TEXT("ResourceDescription_%d"), RowIndex), 20, FLinearColor(0.34f, 0.3f, 0.24f, 1.0f));
	Description->SetText(FText::Format(NSLOCTEXT("RokModal", "ResourceDescription", "使用后获得{0}{1}"), FormatNumber(Amount), ResourceName));
	PlaceOnCanvas(Row, Description, FVector2D(158.4f, 72.1f), FVector2D(590.0f, 27.2f), 2);
	if (OwnedCount > 0)
	{
		UTextBlock* CountText = MakeModalText(*FString::Printf(TEXT("ResourceOwned_%d"), RowIndex), 20, FLinearColor(0.34f, 0.3f, 0.24f, 1.0f));
		CountText->SetText(FText::Format(NSLOCTEXT("RokModal", "OwnedCount", "拥有数量:{0}"), FText::AsNumber(OwnedCount)));
		PlaceOnCanvas(Row, CountText, FVector2D(158.4f, 106.4f), FVector2D(590.0f, 27.2f), 2);
		UButton* UseButton = MakeTextAtlasButton(*FString::Printf(TEXT("ResourceUseButton_%d"), RowIndex), CommonAtlasPath, ResourceBlueButtonRect, CommonAtlasSize, FVector2D(202.0f, 84.0f), NSLOCTEXT("RokModal", "Use", "使用"), 26, FLinearColor::White);
		UseButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandlePreviewItemAction);
		PlaceOnCanvas(Row, UseButton, FVector2D(753.0f, 48.0f), FVector2D(202.0f, 84.0f), 3);
	}
	else
	{
		UButton* BuyButton = MakeTextAtlasButton(*FString::Printf(TEXT("ResourceBuyButton_%d"), RowIndex), CommonAtlasPath, ResourceGoldButtonRect, CommonAtlasSize, FVector2D(202.0f, 84.0f), NSLOCTEXT("RokModal", "BuyAndUse", "购买并使用"), 21, FLinearColor::White, FText::AsNumber(ShortcutPrice));
		BuyButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandlePreviewItemAction);
		PlaceOnCanvas(Row, BuyButton, FVector2D(753.0f, 48.0f), FVector2D(202.0f, 84.0f), 3);
		PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("ResourceGem_%d"), RowIndex), CommonAtlasPath, GemCurrencyRect, CommonAtlasSize, FVector2D(28.0f)), FVector2D(822.0f, 91.0f), FVector2D(28.0f), 5);
	}
	return Row;
}

UButton* URokUnityModalWidget::MakeResourceTab(int32 CategoryIndex, bool bSelected)
{
	UButton* Button = WidgetTree->ConstructWidget<URokUiAnimatedButton>(URokUiAnimatedButton::StaticClass(), *FString::Printf(TEXT("ResourceTab_%d"), CategoryIndex));
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
	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("ResourceTabContent_%d"), CategoryIndex));
	Button->SetContent(Content);
	if (bSelected)
	{
		PlaceOnCanvas(Content, MakeAtlasImage(*FString::Printf(TEXT("ResourceTabSelected_%d"), CategoryIndex), CommonAtlasPath, TabSelectedRect, CommonAtlasSize, FVector2D(144.0f, 103.0f)), FVector2D(0.0f, -2.0f), FVector2D(144.0f, 103.0f));
		PlaceOnCanvas(Content, MakeAtlasImage(*FString::Printf(TEXT("ResourceTabIconSelected_%d"), CategoryIndex), PageAtlasPath, TabIconRects[CategoryIndex][0], PageAtlasSize, FVector2D(60.0f)), FVector2D(31.0f, 19.0f), FVector2D(60.0f), 1);
	}
	else
	{
		PlaceOnCanvas(Content, MakeAtlasImage(*FString::Printf(TEXT("ResourceTabDark_%d"), CategoryIndex), CommonAtlasPath, TabDarkRect, CommonAtlasSize, FVector2D(121.0f, 102.0f)), FVector2D(0.0f, -1.0f), FVector2D(121.0f, 102.0f));
		PlaceOnCanvas(Content, MakeAtlasImage(*FString::Printf(TEXT("ResourceTabIconDark_%d"), CategoryIndex), PageAtlasPath, TabIconRects[CategoryIndex][1], PageAtlasSize, FVector2D(60.0f)), FVector2D(31.0f, 19.0f), FVector2D(60.0f), 1);
	}
	return Button;
}

void URokUnityModalWidget::BuildBuildingInfo()
{
	URokBuildingUpgradeSubsystem* Subsystem = GetUpgradeSubsystem();
	FRokBuildingState State;
	if (Subsystem && SelectedBuildingActor.IsValid())
	{
		State = Subsystem->GetBuildingStateForActor(SelectedBuildingActor.Get());
	}
	else
	{
		State.DisplayName = NSLOCTEXT("RokModal", "BuilderHutPreviewName", "工人小屋");
		State.Type = ERokBuildingType::BuilderHut;
		State.Level = 1;
		State.Power = 1;
	}

	const int32 ResourceType = ResourceBuildingReferenceType != 0 ? ResourceBuildingReferenceType : State.UnityBuildingTypeId;
	if (ResourceType >= 2 && ResourceType <= 5)
	{
		BuildResourceBuildingInfo(ResourceType, SelectedBuildingActor.IsValid() && Subsystem ? State.Level : 0);
		return;
	}

	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("BuildingInfoPreviewFrame"), CommonAtlasPath, PreviewFrameRect, CommonAtlasSize, FVector2D(450.0f, 620.2f), PreviewFrameMargin), FVector2D(208.8f, 166.9f), FVector2D(450.0f, 620.2f));
	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("BuildingInfoStarA"), BuildStarPath, SpriteRect(0, 0, 256, 256), StarTextureSize, FVector2D(420.0f)), FVector2D(223.0f, 260.0f), FVector2D(420.0f), 1);

	UTexture2D* BuildingTexture = nullptr;
	if (State.Type == ERokBuildingType::BuilderHut)
	{
		BuildingTexture = LoadUiTexture(WorkerBuildingPath);
	}
	if (!BuildingTexture && !State.Icon.IsNull())
	{
		BuildingTexture = State.Icon.LoadSynchronous();
	}
	if (BuildingTexture)
	{
		UImage* BuildingImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BuildingInfoImage"));
		FSlateBrush Brush;
		Brush.SetResourceObject(BuildingTexture);
		Brush.ImageSize = FVector2D(500.0f);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		BuildingImage->SetBrush(Brush);
		BuildingImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(ModeCanvas, BuildingImage, FVector2D(188.8f, 257.0f), FVector2D(500.0f), 2);
	}

	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("BuildingInfoAttributeFrame"), CommonAtlasPath, AttributeFrameRect, CommonAtlasSize, FVector2D(536.0f, 144.0f), CommonPanelMargin), FVector2D(682.0f, 205.0f), FVector2D(536.0f, 144.0f));
	const int32 QueueCount = Subsystem ? Subsystem->GetOwnedBuildQueueCount() : 1;
	const float BuildSpeedBonus = Subsystem ? Subsystem->GetBuildSpeedBonusPercent() : 0.0f;
	const FText Labels[3] = {
		NSLOCTEXT("RokModal", "BuildQueue", "建造队列"),
		NSLOCTEXT("RokModal", "BuildingPower", "战力"),
		NSLOCTEXT("RokModal", "BuildSpeedBonus", "建造速度提升"),
	};
	const FText Values[3] = {
		FText::AsNumber(QueueCount),
		FText::AsNumber(State.Power),
		FText::FromString(FString::Printf(TEXT("%.0f%%"), BuildSpeedBonus)),
	};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UTextBlock* Label = MakeModalText(*FString::Printf(TEXT("BuildingInfoStatLabel_%d"), Index), 20, FLinearColor(0.34f, 0.3f, 0.24f, 1.0f));
		Label->SetText(Labels[Index]);
		PlaceOnCanvas(ModeCanvas, Label, FVector2D(704.0f, 218.0f + Index * 40.0f), FVector2D(330.0f, 32.0f), 2);
		UTextBlock* Value = MakeModalText(*FString::Printf(TEXT("BuildingInfoStatValue_%d"), Index), 20, FLinearColor(0.2f, 0.17f, 0.12f, 1.0f), true, ETextJustify::Right);
		Value->SetText(Values[Index]);
		PlaceOnCanvas(ModeCanvas, Value, FVector2D(1045.0f, 218.0f + Index * 40.0f), FVector2D(145.0f, 32.0f), 2);
	}

	UTextBlock* Description = MakeModalText(TEXT("BuildingInfoDescription"), 18, FLinearColor(0.3f, 0.27f, 0.22f, 1.0f));
	Description->SetAutoWrapText(true);
	Description->SetLineHeightPercentage(1.25f);
	if (State.Type == ERokBuildingType::BuilderHut || !SelectedBuildingActor.IsValid())
	{
		Description->SetText(NSLOCTEXT("RokModal", "BuilderHutDescription", "工人小屋是勤劳的建筑工人们日常生活和休息的地方，同时也是城市中唯一可以对建造队列进行管理的建筑物。在这里，您可以选择花费钻石雇佣更多的工人来增加一个额外的建造队列，也可以通过使用加速道具来减少建造队列所需的时间。"));
	}
	else
	{
		Description->SetText(FText::Format(NSLOCTEXT("RokModal", "GenericBuildingDescription", "{0}是城市发展体系中的重要建筑。当前等级为{1}级，可通过升级提升其战力与功能。"), State.DisplayName, FText::AsNumber(State.Level)));
	}
	PlaceOnCanvas(ModeCanvas, Description, FVector2D(704.0f, 386.0f), FVector2D(490.0f, 289.45f), 2);
}

void URokUnityModalWidget::BuildResourceBuildingInfo(int32 Type, int32 KnownLevel)
{
	TArray<FString> Headers; TArray<FRokBuildingLevelDetailsRow> Rows; FString Error, Name, Description;
	if (!RokBuildingLevelDetails::Load(Type, Headers, Rows, Error, &Name, &Description))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		auto* Missing = MakeModalText(TEXT("BuildingLevelDetailsMissing"), 22, FLinearColor::Red);
		Missing->SetText(NSLOCTEXT("RokModal", "BuildingLevelMissing", "建筑等级配置缺失"));
		PlaceOnCanvas(ModeCanvas, Missing, FVector2D(400, 350), FVector2D(650, 60));
		return;
	}
	BuildingDescriptionRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingDescriptionRoot"));
	PlaceOnCanvas(ModeCanvas, BuildingDescriptionRoot, FVector2D::ZeroVector, FVector2D(1440, 900), 3);
	BuildingLevelDetailsRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingLevelDetailsRoot"));
	PlaceOnCanvas(ModeCanvas, BuildingLevelDetailsRoot, FVector2D::ZeroVector, FVector2D(1440, 900), 3);

	PlaceOnCanvas(BuildingDescriptionRoot, MakeAtlasBorder(TEXT("BuildingInfoPreviewFrame"), CommonAtlasPath, PreviewFrameRect, CommonAtlasSize, FVector2D(450, 620.2f), PreviewFrameMargin), FVector2D(208.8f, 166.9f), FVector2D(450, 620.2f));
	// A reference preview has no current actor/level/icon. Do not synthesize an owned building.
	if (SelectedBuildingActor.IsValid())
	{
		if (auto* Upgrade = GetUpgradeSubsystem())
		{
			const auto State = Upgrade->GetBuildingStateForActor(SelectedBuildingActor.Get());
			if (auto* Texture = State.Icon.LoadSynchronous())
			{
				auto* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BuildingInfoImage"));
				FSlateBrush Brush; Brush.SetResourceObject(Texture); Brush.ImageSize = FVector2D(500);
				Icon->SetBrush(Brush); Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
				PlaceOnCanvas(BuildingDescriptionRoot, Icon, FVector2D(188.8f, 257), FVector2D(500), 2);
			}
		}
	}
	PlaceOnCanvas(BuildingDescriptionRoot, MakeAtlasBorder(TEXT("BuildingInfoAttributeFrame"), CommonAtlasPath, AttributeFrameRect, CommonAtlasSize, FVector2D(536, 144), CommonPanelMargin), FVector2D(682, 205), FVector2D(536, 144));
	for (int32 Column = 1; Column < 4; ++Column)
	{
		auto* Label = MakeModalText(*FString::Printf(TEXT("BuildingInfoStatLabel_%d"), Column), 20, FLinearColor(0.34f, 0.3f, 0.24f));
		Label->SetText(FText::FromString(Headers[Column]));
		PlaceOnCanvas(BuildingDescriptionRoot, Label, FVector2D(704, 218 + (Column - 1) * 40), FVector2D(330, 32));
		auto* Value = MakeModalText(*FString::Printf(TEXT("BuildingInfoStatValue_%d"), Column), 20, FLinearColor(0.2f, 0.17f, 0.12f), true, ETextJustify::Right);
		// Current production and capacity depend on buffs/server state, unlike static level rows.
		Value->SetText(Column == 3 && Rows.IsValidIndex(KnownLevel - 1) ? FText::AsNumber(Rows[KnownLevel - 1].Power) : FText::FromString(TEXT("—")));
		PlaceOnCanvas(BuildingDescriptionRoot, Value, FVector2D(1045, 218 + (Column - 1) * 40), FVector2D(145, 32));
	}
	auto* Desc = MakeModalText(TEXT("BuildingInfoDescription"), 18, FLinearColor(0.3f, 0.27f, 0.22f));
	Desc->SetAutoWrapText(true); Desc->SetText(FText::FromString(Description));
	PlaceOnCanvas(BuildingDescriptionRoot, Desc, FVector2D(704, 386), FVector2D(490, 289.45f));
	auto* Info = MakeAtlasButton(TEXT("BuildingLevelInfoButton"), CommonAtlasPath, HelpRect, CommonAtlasSize, FVector2D(38));
	FButtonStyle InfoStyle = Info->GetStyle();
	InfoStyle.SetNormalPadding(FMargin(11)); InfoStyle.SetPressedPadding(FMargin(11)); Info->SetStyle(InfoStyle);
	Info->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleBuildingLevelInfoClicked);
	PlaceOnCanvas(BuildingDescriptionRoot, Info, FVector2D(222.05f, 272), FVector2D(60), 5);
	auto* Back = MakeAtlasButton(TEXT("BuildingLevelBackButton"), CommonAtlasPath, SpriteRect(703, 282, 69, 69), CommonAtlasSize, FVector2D(69));
	Back->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleBuildingLevelBackClicked);
	BuildingLevelBackButton = Back;
	PlaceOnCanvas(ModeCanvas, Back, FVector2D(191.5f, 99.5f), FVector2D(69), 5);

	// Chinese ArabLayout Awake flips serialized +/-400,+/-130 into source column order.
	const float ColumnX[4] = { -13, 257, 517, 787 };
	PlaceOnCanvas(BuildingLevelDetailsRoot, MakeAtlasImage(TEXT("BuildingLevelHeaderLine"), CommonAtlasPath, SpriteRect(1003, 976, 964, 47), CommonAtlasSize, FVector2D(964, 43)), FVector2D(238, 184.2f), FVector2D(964, 43));
	for (int32 Column = 0; Column < 4; ++Column)
	{
		auto* Header = MakeModalText(*FString::Printf(TEXT("BuildingLevelHeader_%d"), Column), 20, FLinearColor(0.34f, 0.3f, 0.24f), true, ETextJustify::Center);
		Header->SetText(FText::FromString(Headers[Column]));
		PlaceOnCanvas(BuildingLevelDetailsRoot, Header, FVector2D(208 + ColumnX[Column], 180.7f), FVector2D(250, 50), 2);
	}
	BuildingLevelDetailsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BuildingLevelDetailsScroll"));
	BuildingLevelDetailsScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(BuildingLevelDetailsRoot, BuildingLevelDetailsScroll, FVector2D(208, 230.7f), FVector2D(1024, 542.8f));
	auto* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BuildingLevelRowsSize"));
	Size->SetWidthOverride(1024); Size->SetHeightOverride(1250);
	auto* Grid = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingLevelRows"));
	Size->SetContent(Grid); BuildingLevelDetailsScroll->AddChild(Size);
	for (const auto& Row : Rows)
	{
		const float Y = (Row.Level - 1) * 50.0f;
		if (KnownLevel == Row.Level)
			PlaceOnCanvas(Grid, MakeAtlasImage(TEXT("BuildingLevelCurrentRow"), CommonAtlasPath, SpriteRect(1, 885, 964, 53), CommonAtlasSize, FVector2D(964, 53)), FVector2D(30, Y - 1.5f), FVector2D(964, 53));
		PlaceOnCanvas(Grid, MakeAtlasImage(*FString::Printf(TEXT("BuildingLevelLine_%d"), Row.Level), CommonAtlasPath, SpriteRect(1, 611, 570, 2), CommonAtlasSize, FVector2D(964, 3)), FVector2D(30, Y + 47), FVector2D(964, 3));
		const int32 Values[4] = { Row.Level, Row.ProduceSpeed, Row.GatherMax, Row.Power };
		for (int32 Column = 0; Column < 4; ++Column)
		{
			auto* Cell = MakeModalText(*FString::Printf(TEXT("BuildingLevelCell_%d_%d"), Row.Level, Column), 20, FLinearColor(0.3f, 0.27f, 0.22f), false, ETextJustify::Center);
			Cell->SetText(FText::AsNumber(Values[Column]));
			PlaceOnCanvas(Grid, Cell, FVector2D(ColumnX[Column], Y), FVector2D(250, 50), 2);
		}
	}
	ApplyBuildingPageAnimation();
}

void URokUnityModalWidget::HandleBuildingLevelInfoClicked()
{
	if (ActiveMode != ERokUnityModalMode::BuildingInfo || !BuildingLevelDetailsRoot || bBuildingLevelPage) return;
	bBuildingLevelPage = true; BuildingPageTime = 0;
	ApplyBuildingPageAnimation();
}

void URokUnityModalWidget::HandleBuildingLevelBackClicked()
{
	if (ActiveMode != ERokUnityModalMode::BuildingInfo || !BuildingDescriptionRoot || !bBuildingLevelPage) return;
	bBuildingLevelPage = false; BuildingPageTime = 0;
	ApplyBuildingPageAnimation();
}

void URokUnityModalWidget::ApplyBuildingPageAnimation()
{
	if (!BuildingDescriptionRoot || !BuildingLevelDetailsRoot) return;
	if (BuildingLevelBackButton) BuildingLevelBackButton->SetVisibility(bBuildingLevelPage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	BuildingDescriptionRoot->SetVisibility(bBuildingLevelPage ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	BuildingLevelDetailsRoot->SetVisibility(bBuildingLevelPage ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	auto* Target = bBuildingLevelPage ? BuildingLevelDetailsRoot.Get() : BuildingDescriptionRoot.Get();
	const auto Sample = RokBuildingLevelDetails::Animate(BuildingPageTime, bBuildingLevelPage);
	Target->SetRenderTranslation(FVector2D(Sample.X, 0)); Target->SetRenderOpacity(Sample.Alpha);
}

void URokUnityModalWidget::BuildBuildingUpgrade()
{
	FRokBuildingUpgradePreview Preview;
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetUpgradeSubsystem();
	const bool bIsSourcePreview = !SelectedBuildingActor.IsValid();
	const bool bHasRuntimePreview = !bIsSourcePreview
		&& UpgradeSubsystem
		&& UpgradeSubsystem->GetUpgradePreview(SelectedBuildingActor.Get(), Preview);
	if (bIsSourcePreview)
	{
		// Source-derived farm level-1 -> level-2 preview used by the screenshot
		// command-line acceptance path. It is not persisted as player state.
		Preview.CurrentState.DisplayName = NSLOCTEXT("RokModal", "FarmPreviewName", "农场");
		Preview.CurrentState.Type = ERokBuildingType::Farm;
		Preview.CurrentState.Level = 1;
		Preview.TargetLevel = 2;
		Preview.CurrentPower = 5;
		Preview.NextPower = 11;
		Preview.CurrentProducePerHour = 400;
		Preview.NextProducePerHour = 430;
		Preview.CurrentGatherMax = 4000;
		Preview.NextGatherMax = 4300;
		Preview.Cost.Wood = 200;
		Preview.BuildSeconds = 15;
		Preview.bCanStartTimedUpgrade = false;
	}
	else if (!bHasRuntimePreview && UpgradeSubsystem && Preview.CurrentState.DisplayName.IsEmpty())
	{
		// A real selected actor must never be replaced with the farm acceptance
		// preview, even if its config has no next level.
		Preview.CurrentState = UpgradeSubsystem->GetBuildingStateForActor(SelectedBuildingActor.Get());
		Preview.TargetLevel = Preview.CurrentState.Level;
		Preview.CurrentPower = Preview.CurrentState.Power;
		Preview.NextPower = Preview.CurrentState.Power;
		Preview.CurrentProducePerHour = Preview.CurrentState.ProducePerHour;
		Preview.NextProducePerHour = Preview.CurrentState.ProducePerHour;
		Preview.CurrentGatherMax = Preview.CurrentState.GatherMax;
		Preview.NextGatherMax = Preview.CurrentState.GatherMax;
	}

	const bool bDeclaredBarracksPreview = !bIsSourcePreview && Preview.CurrentState.UnityBuildingTypeId == 9;
	UCanvasPanel* UpgradeContentCanvas = ModeCanvas;
	if (bDeclaredBarracksPreview)
	{
		BuildingDescriptionRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BarracksLevelDescriptionRoot"));
		PlaceOnCanvas(ModeCanvas, BuildingDescriptionRoot, FVector2D::ZeroVector, FVector2D(1440, 900), 1);
		UpgradeContentCanvas = BuildingDescriptionRoot;
	}
	PlaceOnCanvas(UpgradeContentCanvas, MakeAtlasBorder(TEXT("BuildingUpgradePreviewFrame"), CommonAtlasPath, PreviewFrameRect, CommonAtlasSize, FVector2D(450.0f, 620.0f), PreviewFrameMargin), bDeclaredBarracksPreview ? FVector2D(203.0f, 110.3f) : FVector2D(203.9f, 166.9f), FVector2D(450.0f, 620.0f));
	if (bDeclaredBarracksPreview)
	{
		RokBuildingPreviewGlow::FData GlowData;
		FString GlowError;
		if (GlowData.Load(RokBuildingPreviewGlow::FData::DefaultPath(), GlowError))
		{
			auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RokPrototype/UI/Materials/M_RokBuildingPreviewGlowUI.M_RokBuildingPreviewGlowUI"));
			auto* Glow = WidgetTree->ConstructWidget<URokBuildingPreviewGlowWidget>(URokBuildingPreviewGlowWidget::StaticClass(), TEXT("BuildingPreviewGlowWidget"));
			if (Glow->Initialize(GlowData, Material, GlowError))
			{
				// Source coordinates already include the native window ancestry.
				PlaceOnCanvas(UpgradeContentCanvas, Glow, FVector2D::ZeroVector, FVector2D(1440, 810), 1);
				Glow->SetPlaybackActive(true);
			}
		}
		if (!GlowError.IsEmpty())
			UE_LOG(LogTemp, Error, TEXT("Barracks source preview glow unavailable: %s"), *GlowError);
	}
	else
	{
		UImage* StarA = MakeAtlasImage(TEXT("BuildingUpgradeStarA"), BuildStarPath, SpriteRect(0, 0, 256, 256), StarTextureSize, FVector2D(430.0f));
		StarA->SetColorAndOpacity(FLinearColor(1.0f, 0.78f, 0.25f, 0.55f));
		StarA->SetRenderTransformAngle(12.0f);
		PlaceOnCanvas(UpgradeContentCanvas, StarA, FVector2D(213.0f, 260.0f), FVector2D(430.0f), 1);
		UImage* StarB = MakeAtlasImage(TEXT("BuildingUpgradeStarB"), BuildStarPath, SpriteRect(0, 0, 256, 256), StarTextureSize, FVector2D(360.0f));
		StarB->SetColorAndOpacity(FLinearColor(1.0f, 0.68f, 0.16f, 0.32f));
		StarB->SetRenderTransformAngle(-18.0f);
		PlaceOnCanvas(UpgradeContentCanvas, StarB, FVector2D(248.0f, 295.0f), FVector2D(360.0f), 1);
	}

	if (bDeclaredBarracksPreview)
	{
		FString Model, Error;
		if (RokBuildingSourcePreview::ResolveActorModel(SelectedBuildingActor.Get(), Model, Error))
		{
			// This texture is the verified raw source of the current scene presentation.
			// Resolving the scene model does not assign a civilization to the player.
			UTexture2D* RawTexture = Model == TEXT("build_9_101_1")
				? LoadUiTexture(TEXT("/Game/RokPrototype/Textures/RuntimeCity/UnityGameApp_build_9_101_1.UnityGameApp_build_9_101_1")) : nullptr;
			auto* SourcePreview = WidgetTree->ConstructWidget<URokBuildingSourcePreviewWidget>(URokBuildingSourcePreviewWidget::StaticClass(), TEXT("BuildingSourcePreviewWidget"));
			if (SourcePreview->InitializePreview(Model, RawTexture))
				PlaceOnCanvas(UpgradeContentCanvas, SourcePreview, SourcePreview->GetSourcePosition(), SourcePreview->GetSourceSize(), 2);
			else
				Error = SourcePreview->GetLoadError();
		}
		if (!Error.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Barracks scene presentation unavailable: %s"), *Error);
			UTextBlock* Unavailable = MakeModalText(TEXT("BuildingSourcePreviewUnavailable"), 15, FLinearColor::White);
			Unavailable->SetText(NSLOCTEXT("RokModal", "BuildingSourcePreviewUnavailable", "建筑外观源数据不可用"));
			PlaceOnCanvas(UpgradeContentCanvas, Unavailable, FVector2D(248.8, 440), FVector2D(358.4, 40), 2);
		}
	}
	else
	{
		UTexture2D* BuildingTexture = Preview.CurrentState.Type == ERokBuildingType::Farm
			? LoadUiTexture(FarmBuildingPath)
			: nullptr;
		if (!BuildingTexture && !Preview.CurrentState.Icon.IsNull())
		{
			BuildingTexture = Preview.CurrentState.Icon.LoadSynchronous();
		}
		if (BuildingTexture)
		{
			UImage* BuildingImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BuildingUpgradeImage"));
			FSlateBrush Brush;
			Brush.SetResourceObject(BuildingTexture);
			Brush.ImageSize = FVector2D(500.0f);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			BuildingImage->SetBrush(Brush);
			BuildingImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			PlaceOnCanvas(UpgradeContentCanvas, BuildingImage, FVector2D(177.9f, 257.0f), FVector2D(500.0f), 2);
		}
	}
	if (bDeclaredBarracksPreview)
	{
		// Unity btn_BuildLevelInfo has a 60x60 GameButton hitbox and an inset 38x38 btn_com_1007 glyph.
		auto* Info = MakeAtlasButton(TEXT("BarracksLevelInfoButton"), CommonAtlasPath, HelpRect, CommonAtlasSize, FVector2D(38));
		FButtonStyle InfoStyle = Info->GetStyle();
		InfoStyle.SetNormalPadding(FMargin(11));
		InfoStyle.SetPressedPadding(FMargin(11));
		Info->SetStyle(InfoStyle);
		// UButtonSlot defaults to (4,2); clear it so the 11px style inset alone
		// places the child glyph at Unity's (222,238) with its full 38x38 size.
		if (UButtonSlot* InfoSlot = Cast<UButtonSlot>(Info->GetContentSlot()))
		{
			InfoSlot->SetPadding(FMargin(0));
		}
		Info->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleBarracksLevelInfoClicked);
		PlaceOnCanvas(UpgradeContentCanvas, Info, FVector2D(211, 227), FVector2D(60), 5);
	}
	else
	{
		PlaceOnCanvas(UpgradeContentCanvas, MakeAtlasImage(TEXT("BuildingUpgradeInfoIcon"), CommonAtlasPath, HelpRect, CommonAtlasSize, FVector2D(38.0f)), FVector2D(586.0f, 223.0f), FVector2D(38.0f), 3);
	}

	const bool bBarracksAttributes = !bIsSourcePreview && URokBarracksUpgradeAttributes::Supports(Preview.CurrentState.UnityBuildingTypeId, Preview.CurrentState.Level, Preview.TargetLevel);
	// ClearChildren detaches the previous page but its UWidgets may still have
	// WidgetTree as Outer. Retire only the two names whose class changes between
	// source barracks glyphs and regular building level text. The new visible
	// widgets keep the stable names used by audits and widget lookups.
	auto RetireConflictingLevelWidget = [this](const TCHAR* WidgetName, UClass* DesiredClass)
	{
		UWidget* Existing = FindObjectFast<UWidget>(WidgetTree, FName(WidgetName));
		if (!Existing || Existing->IsA(DesiredClass)) return true;
		if (Existing->Rename(nullptr, WidgetTree, REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty)) return true;
		UE_LOG(LogTemp, Error, TEXT("Could not retire detached upgrade widget %s before class change."), WidgetName);
		return false;
	};
	UClass* LevelClass = bBarracksAttributes ? URokBarracksSourceTextWidget::StaticClass() : UTextBlock::StaticClass();
	if (!RetireConflictingLevelWidget(TEXT("UpgradeCurrentLevel"), LevelClass)
		|| !RetireConflictingLevelWidget(TEXT("UpgradeNextLevel"), LevelClass)) return;
	if (!bBarracksAttributes)
	PlaceOnCanvas(UpgradeContentCanvas, MakeAtlasBorder(TEXT("BuildingUpgradeTopFrame"), CommonAtlasPath, AttributeFrameRect, CommonAtlasSize, FVector2D(536.0f, 281.74f), CommonPanelMargin), FVector2D(670.0f, 188.0f), FVector2D(536.0f, 281.74f));
	if (bBarracksAttributes)
	{
		FString GlyphError;
		auto* SourceTextData = NewObject<URokBarracksSourceTextData>(this);
		if (!SourceTextData->Load(GlyphError))
			UE_LOG(LogTemp, Error, TEXT("Barracks source level glyphs: %s"), *GlyphError);
		for (int32 LevelIndex = 0; LevelIndex < 2; ++LevelIndex)
		{
			const bool bNext = LevelIndex == 1;
			auto* LevelText = WidgetTree->ConstructWidget<URokBarracksSourceTextWidget>(URokBarracksSourceTextWidget::StaticClass(), bNext ? TEXT("UpgradeNextLevel") : TEXT("UpgradeCurrentLevel"));
			LevelText->Configure(SourceTextData, 24, true, 3, URokBarracksUpgradeAttributes::LevelSize(bNext), FLinearColor::White);
			if (!LevelText->SetText(bNext ? FText::AsNumber(Preview.TargetLevel) : FText::Format(NSLOCTEXT("RokModal", "BarracksSourceLevel", "等级{0}"), FText::AsNumber(Preview.CurrentState.Level))))
				UE_LOG(LogTemp, Error, TEXT("Barracks source level glyph missing for %s"), *LevelText->GetText().ToString());
			UBorder* Region = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), bNext ? TEXT("UpgradeNextLevelRegion") : TEXT("UpgradeCurrentLevelRegion"));
			Region->SetBrushColor(FLinearColor::Transparent); Region->SetPadding(FMargin(0)); Region->SetVerticalAlignment(VAlign_Center); Region->SetContent(LevelText);
			PlaceOnCanvas(UpgradeContentCanvas, Region, URokBarracksUpgradeAttributes::LevelPosition(bNext), URokBarracksUpgradeAttributes::LevelSize(bNext), 3);
		}
		UImage* Arrow = MakeAtlasImage(TEXT("UpgradeLevelArrow"), CommonAtlasPath, LevelArrowRect, CommonAtlasSize, FVector2D(24, 26));
		Arrow->SetRenderTransformPivot(FVector2D(.5, .5)); Arrow->SetRenderScale(FVector2D(-1, 1));
		PlaceOnCanvas(UpgradeContentCanvas, Arrow, FVector2D(779.3, 193.82), FVector2D(24, 26), 3);
	}
	else
	{
		UTextBlock* CurrentLevel = MakeModalText(TEXT("UpgradeCurrentLevel"), 24, FLinearColor(0.27f, 0.23f, 0.16f, 1.0f), true, ETextJustify::Right);
		CurrentLevel->SetText(FText::AsNumber(Preview.CurrentState.Level));
		PlaceOnCanvas(UpgradeContentCanvas, CurrentLevel, FVector2D(832.0f, 206.0f), FVector2D(80.0f, 42.7f), 2);
		PlaceOnCanvas(UpgradeContentCanvas, MakeAtlasImage(TEXT("UpgradeLevelArrow"), CommonAtlasPath, LevelArrowRect, CommonAtlasSize, FVector2D(24.0f, 26.0f)), FVector2D(922.0f, 214.0f), FVector2D(24.0f, 26.0f), 2);
		UTextBlock* NextLevel = MakeModalText(TEXT("UpgradeNextLevel"), 24, FLinearColor(0.2f, 0.62f, 0.18f, 1.0f), true);
		NextLevel->SetText(FText::AsNumber(Preview.TargetLevel));
		PlaceOnCanvas(UpgradeContentCanvas, NextLevel, FVector2D(956.0f, 206.0f), FVector2D(80.0f, 42.7f), 2);
	
	}

	if (bBarracksAttributes)
	{
		auto* Attributes = WidgetTree->ConstructWidget<URokBarracksUpgradeAttributes>(URokBarracksUpgradeAttributes::StaticClass(), TEXT("BarracksUpgradeAttributes"));
		if (Attributes->InitializeAttributes())
		{
			PlaceOnCanvas(UpgradeContentCanvas, Attributes, URokBarracksUpgradeAttributes::SourcePosition(), FVector2D(536, Attributes->GetSourceHeight()), 2);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Barracks upgrade attributes: %s"), *Attributes->GetLoadError());
			UTextBlock* Error = MakeModalText(TEXT("BarracksUpgradeAttributeUnavailable"), 15, FLinearColor::White);
			Error->SetText(NSLOCTEXT("RokModal", "BarracksAttributeUnavailable", "兵营升级属性源数据不可用"));
			PlaceOnCanvas(UpgradeContentCanvas, Error, URokBarracksUpgradeAttributes::SourcePosition(), FVector2D(536, 110), 2);
		}
	}
	else
	{
		struct FUpgradeStatRow
		{
			FText Label;
			int32 Current = 0;
			int32 Next = 0;
		};
		const FUpgradeStatRow Rows[3] = {
			{NSLOCTEXT("RokModal", "FoodPerHour", "每小时食物产量"), Preview.CurrentProducePerHour, Preview.NextProducePerHour},
			{NSLOCTEXT("RokModal", "FoodCapacity", "食物最大收取量"), Preview.CurrentGatherMax, Preview.NextGatherMax},
			{NSLOCTEXT("RokModal", "UpgradePower", "战力"), Preview.CurrentPower, Preview.NextPower},
		};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const float Y = 263.0f + Index * 54.0f;
			UTextBlock* Label = MakeModalText(*FString::Printf(TEXT("UpgradeStatLabel_%d"), Index), 20, FLinearColor(0.33f, 0.29f, 0.23f, 1.0f));
			Label->SetText(Rows[Index].Label);
			PlaceOnCanvas(UpgradeContentCanvas, Label, FVector2D(700.0f, Y), FVector2D(270.0f, 36.0f), 2);
			UTextBlock* Current = MakeModalText(*FString::Printf(TEXT("UpgradeStatCurrent_%d"), Index), 20, FLinearColor(0.25f, 0.22f, 0.17f, 1.0f), true, ETextJustify::Right);
			Current->SetText(FormatNumber(Rows[Index].Current));
			PlaceOnCanvas(UpgradeContentCanvas, Current, FVector2D(955.0f, Y), FVector2D(90.0f, 36.0f), 2);
			UTextBlock* Delta = MakeModalText(*FString::Printf(TEXT("UpgradeStatDelta_%d"), Index), 20, FLinearColor(0.15f, 0.65f, 0.16f, 1.0f), true);
			Delta->SetText(FText::FromString(FString::Printf(TEXT("+%s"), *FormatNumber(FMath::Max(0, Rows[Index].Next - Rows[Index].Current)).ToString())));
			PlaceOnCanvas(UpgradeContentCanvas, Delta, FVector2D(1060.0f, Y), FVector2D(120.0f, 36.0f), 2);
		}
	
	}

	PlaceOnCanvas(UpgradeContentCanvas, MakeAtlasBorder(TEXT("UpgradeResourceFrame"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(536.0f, 68.0f), CommonPanelMargin), FVector2D(670.0f, 488.0f), FVector2D(536.0f, 68.0f));
	struct FCostVisual
	{
		int32 Cost = 0;
		FIntRect IconRect;
	};
	const FCostVisual Costs[4] = {
		{Preview.Cost.Food, FoodCurrencyRect},
		{Preview.Cost.Wood, WoodCurrencyRect},
		{Preview.Cost.Stone, StoneCurrencyRect},
		{Preview.Cost.Gold, GoldCurrencyRect},
	};
	int32 CostVisualIndex = 0;
	for (const FCostVisual& Cost : Costs)
	{
		if (Cost.Cost <= 0)
		{
			continue;
		}
		const float X = 686.0f + CostVisualIndex * 126.0f;
		PlaceOnCanvas(UpgradeContentCanvas, MakeAtlasImage(*FString::Printf(TEXT("UpgradeCostIcon_%d"), CostVisualIndex), CommonAtlasPath, Cost.IconRect, CommonAtlasSize, FVector2D(46.0f)), FVector2D(X, 499.0f), FVector2D(46.0f), 2);
		UTextBlock* CostText = MakeModalText(*FString::Printf(TEXT("UpgradeCostText_%d"), CostVisualIndex), 20, FLinearColor(0.27f, 0.23f, 0.16f, 1.0f), true);
		CostText->SetText(FormatNumber(Cost.Cost));
		PlaceOnCanvas(UpgradeContentCanvas, CostText, FVector2D(X + 46.0f, 505.0f), FVector2D(78.0f, 32.0f), 2);
		++CostVisualIndex;
	}

	UButton* InstantButton = MakeTextAtlasButton(TEXT("InstantUpgradeButton"), CommonAtlasPath, InstantUpgradeButtonRect, CommonAtlasSize, FVector2D(274.0f, 84.0f), NSLOCTEXT("RokModal", "InstantUpgrade", "立即升级"), 26, FLinearColor::White, FText::AsNumber(1));
	InstantButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleInstantUpgradePreview);
	PlaceOnCanvas(UpgradeContentCanvas, InstantButton, FVector2D(660.0f, 650.0f), FVector2D(274.0f, 84.0f), 3);
	PlaceOnCanvas(UpgradeContentCanvas, MakeAtlasImage(TEXT("InstantUpgradeGem"), CommonAtlasPath, GemCurrencyRect, CommonAtlasSize, FVector2D(28.0f)), FVector2D(768.0f, 691.0f), FVector2D(28.0f), 5);

	UButton* TimedButton = MakeTextAtlasButton(TEXT("TimedUpgradeButton"), CommonAtlasPath, TimedUpgradeButtonRect, CommonAtlasSize, FVector2D(274.0f, 84.0f), NSLOCTEXT("RokModal", "TimedUpgrade", "升级"), 26, FLinearColor::White, FText::Format(NSLOCTEXT("RokModal", "Seconds", "{0}秒"), FText::AsNumber(Preview.BuildSeconds)));
	// Source-only screenshot previews stay visually faithful while the handler
	// still refuses to mutate state without a real selected building actor.
	TimedButton->SetIsEnabled(bIsSourcePreview || (bHasRuntimePreview && Preview.bCanStartTimedUpgrade));
	TimedButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleStartUpgrade);
	PlaceOnCanvas(UpgradeContentCanvas, TimedButton, FVector2D(946.0f, 650.0f), FVector2D(274.0f, 84.0f), 3);

	FeedbackText = MakeModalText(TEXT("UpgradeFeedback"), 16, FLinearColor(0.85f, 0.2f, 0.12f, 1.0f), true, ETextJustify::Center);
	const bool bShowUpgradeFailure = !bIsSourcePreview && bHasRuntimePreview
		&& !Preview.bCanStartTimedUpgrade && !Preview.FailureReason.IsEmpty();
	FeedbackText->SetText(bShowUpgradeFailure ? FText::FromString(Preview.FailureReason) : FText::GetEmpty());
	FeedbackText->SetVisibility(bShowUpgradeFailure ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	PlaceOnCanvas(UpgradeContentCanvas, FeedbackText, FVector2D(670.0f, 608.0f), FVector2D(536.0f, 34.0f), 4);
	if (bDeclaredBarracksPreview)
	{
		BuildBarracksLevelDetails(Preview.CurrentState.Level);
	}
}

void URokUnityModalWidget::BuildBarracksLevelDetails(int32 KnownLevel)
{
	// Unity Text uses pixel size; Slate FontInfo uses points at its 96-DPI renderer.
	constexpr float LevelTextPoints = 22.0f * 72.0f / 96.0f;
	TArray<FString> Headers;
	TArray<FRokBarracksLevelDetailsRow> Rows;
	FString Error;
	if (!RokBuildingLevelDetails::LoadBarracks(Headers, Rows, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Barracks level details: %s"), *Error);
		return;
	}
	auto* HeaderSourceData = NewObject<URokBarracksSourceTextData>(this);
	if (!HeaderSourceData->LoadLevelHeaders(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Barracks level header source glyphs: %s"), *Error);
		if (auto* InfoButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("BarracksLevelInfoButton"))))
		{
			InfoButton->SetIsEnabled(false);
		}
		auto* Missing = MakeModalText(TEXT("BarracksLevelHeaderSourceUnavailable"), 15, FLinearColor::Red);
		Missing->SetText(NSLOCTEXT("RokModal", "BarracksLevelHeaderSourceUnavailable", "兵营等级表头源字形不可用"));
		PlaceOnCanvas(BuildingDescriptionRoot, Missing, FVector2D(700, 570), FVector2D(480, 30), 5);
		return;
	}
	for (int32 Column = 0; Column < 4; ++Column)
	{
		if (!HeaderSourceData->Find(Headers[Column], 22, false, 4, 250))
		{
			UE_LOG(LogTemp, Error, TEXT("Barracks level header source glyph missing: column=%d text=%s"), Column, *Headers[Column]);
			if (auto* InfoButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("BarracksLevelInfoButton"))))
			{
				InfoButton->SetIsEnabled(false);
			}
			auto* Missing = MakeModalText(TEXT("BarracksLevelHeaderSourceUnavailable"), 15, FLinearColor::Red);
			Missing->SetText(NSLOCTEXT("RokModal", "BarracksLevelHeaderSourceUnavailable", "兵营等级表头源字形不可用"));
			PlaceOnCanvas(BuildingDescriptionRoot, Missing, FVector2D(700, 570), FVector2D(480, 30), 5);
			return;
		}
	}
	BarracksLevelSelectedLevel = Rows.IsValidIndex(KnownLevel - 1) ? KnownLevel : 0;
	BarracksLevelDetailsRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BarracksLevelDetailsRoot"));
	PlaceOnCanvas(ModeCanvas, BarracksLevelDetailsRoot, FVector2D::ZeroVector, FVector2D(1440, 900), 1);
	BarracksLevelBackButton = MakeAtlasButton(TEXT("BarracksLevelBackButton"), CommonAtlasPath, SpriteRect(703, 282, 69, 69), CommonAtlasSize, FVector2D(69));
	if (UButtonSlot* BackSlot = Cast<UButtonSlot>(BarracksLevelBackButton->GetContentSlot()))
	{
		BackSlot->SetPadding(FMargin(0));
	}
	BarracksLevelBackButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleBarracksLevelBackClicked);
	PlaceOnCanvas(ModeCanvas, BarracksLevelBackButton, FVector2D(179.5f, 54.5f), FVector2D(69), 5);
	const float ColumnX[4] = { -13, 257, 517, 787 };
	PlaceOnCanvas(BarracksLevelDetailsRoot, MakeAtlasImage(TEXT("BarracksLevelHeaderLine"), CommonAtlasPath, SpriteRect(1003, 976, 964, 47), CommonAtlasSize, FVector2D(964, 43)), FVector2D(238, 136.35f), FVector2D(964, 43));
	for (int32 Column = 0; Column < 4; ++Column)
	{
		// UI_Item_BuildingLevelData_Title_Four: original UGUI mesh, 22px normal, MiddleCenter.
		auto* Header = WidgetTree->ConstructWidget<URokBarracksSourceTextWidget>(URokBarracksSourceTextWidget::StaticClass(), *FString::Printf(TEXT("BarracksLevelHeader_%d"), Column));
		Header->Configure(HeaderSourceData, 22, false, 4, FVector2D(250, 50), FLinearColor::White);
		if (!Header->SetText(FText::FromString(Headers[Column])))
		{
			UE_LOG(LogTemp, Error, TEXT("Barracks level header source glyph binding failed: column=%d"), Column);
		}
		auto* HeaderRegion = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("BarracksLevelHeaderRegion_%d"), Column));
		HeaderRegion->SetBrushColor(FLinearColor::Transparent);
		HeaderRegion->SetPadding(FMargin(0));
		HeaderRegion->SetHorizontalAlignment(HAlign_Fill);
		HeaderRegion->SetVerticalAlignment(VAlign_Center);
		HeaderRegion->SetContent(Header);
		PlaceOnCanvas(BarracksLevelDetailsRoot, HeaderRegion, FVector2D(208 + ColumnX[Column], 132.85f), FVector2D(250, 50), 2);
	}
	BarracksLevelDetailsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BarracksLevelDetailsScroll"));
	BarracksLevelDetailsScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(BarracksLevelDetailsRoot, BarracksLevelDetailsScroll, FVector2D(208, 182.85f), FVector2D(1024, 546.8f));
	auto* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BarracksLevelRowsSize"));
	Size->SetWidthOverride(1024);
	Size->SetHeightOverride(Rows.Num() * 50);
	auto* Grid = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BarracksLevelRows"));
	Size->SetContent(Grid);
	BarracksLevelDetailsScroll->AddChild(Size);
	for (const FRokBarracksLevelDetailsRow& Row : Rows)
	{
		const float Y = (Row.Level - 1) * 50.0f;
		if (KnownLevel == Row.Level)
		{
			PlaceOnCanvas(Grid, MakeAtlasImage(TEXT("BarracksLevelCurrentRow"), CommonAtlasPath, SpriteRect(1, 885, 964, 53), CommonAtlasSize, FVector2D(964, 53)), FVector2D(30, Y - 1.5f), FVector2D(964, 53));
		}
		PlaceOnCanvas(Grid, MakeAtlasImage(*FString::Printf(TEXT("BarracksLevelLine_%d"), Row.Level), CommonAtlasPath, SpriteRect(1, 611, 570, 2), CommonAtlasSize, FVector2D(964, 3)), FVector2D(30, Y + 47), FVector2D(964, 3));
		const FText Values[4] = {
			FText::AsNumber(Row.Level),
			FText::AsNumber(Row.InfantryTrainNumber),
			FText::FromString(Row.InfantryAttackText),
			FText::AsNumber(Row.Power),
		};
		for (int32 Column = 0; Column < 4; ++Column)
		{
			// UI_Item_BuildingLevelData_Info_Four: 22px normal, white, MiddleCenter.
			auto* Cell = MakeModalText(*FString::Printf(TEXT("BarracksLevelCell_%d_%d"), Row.Level, Column), 22, FLinearColor::White, false, ETextJustify::Center);
			auto Font = Cell->GetFont();
			Font.Size = LevelTextPoints;
			Cell->SetFont(Font);
			Cell->SetShadowOffset(FVector2D::ZeroVector);
			Cell->SetText(Values[Column]);
			auto* CellRegion = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("BarracksLevelCellRegion_%d_%d"), Row.Level, Column));
			CellRegion->SetBrushColor(FLinearColor::Transparent);
			CellRegion->SetPadding(FMargin(0));
			CellRegion->SetHorizontalAlignment(HAlign_Fill);
			CellRegion->SetVerticalAlignment(VAlign_Center);
			CellRegion->SetContent(Cell);
			PlaceOnCanvas(Grid, CellRegion, FVector2D(ColumnX[Column], Y), FVector2D(250, 50), 2);
		}
	}
	ApplyBarracksLevelPageAnimation();
}

void URokUnityModalWidget::HandleBarracksLevelInfoClicked()
{
	if (ActiveMode != ERokUnityModalMode::BuildingUpgrade || !SelectedBuildingActor.IsValid()
		|| !BarracksLevelDetailsRoot || bBarracksLevelPage)
	{
		return;
	}
	bBarracksLevelPage = true;
	BarracksLevelPageTime = 0;
	ApplyBarracksLevelPageAnimation();
}

void URokUnityModalWidget::HandleBarracksLevelBackClicked()
{
	if (ActiveMode != ERokUnityModalMode::BuildingUpgrade || !BuildingDescriptionRoot || !bBarracksLevelPage)
	{
		return;
	}
	bBarracksLevelPage = false;
	BarracksLevelPageTime = 0;
	ApplyBarracksLevelPageAnimation();
}

void URokUnityModalWidget::ApplyBarracksLevelPageAnimation()
{
	if (!BuildingDescriptionRoot || !BarracksLevelDetailsRoot) return;
	if (BarracksLevelBackButton) BarracksLevelBackButton->SetVisibility(bBarracksLevelPage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	BuildingDescriptionRoot->SetVisibility(bBarracksLevelPage ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	BarracksLevelDetailsRoot->SetVisibility(bBarracksLevelPage ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	auto* Target = bBarracksLevelPage ? BarracksLevelDetailsRoot.Get() : BuildingDescriptionRoot.Get();
	const auto Sample = RokBuildingLevelDetails::Animate(BarracksLevelPageTime, bBarracksLevelPage);
	Target->SetRenderTranslation(FVector2D(Sample.X, 0));
	Target->SetRenderOpacity(Sample.Alpha);
}

#if !UE_BUILD_SHIPPING
float URokUnityModalWidget::GetBarracksLevelScrollOffsetForAudit() const
{
	return BarracksLevelDetailsScroll ? BarracksLevelDetailsScroll->GetScrollOffset() : 0.0f;
}
#endif

void URokUnityModalWidget::UpdateExpandedDesignResolution(const FVector2D& RootSize)
{
	if (!ExpandedDesignSizeBox || RootSize.X <= 1.0f || RootSize.Y <= 1.0f)
	{
		return;
	}
	const float ViewAspect = RootSize.X / RootSize.Y;
	const float ReferenceAspect = UnityReferenceWidth / UnityReferenceHeight;
	const FVector2D DesiredDesignSize = ViewAspect >= ReferenceAspect
		? FVector2D(UnityReferenceHeight * ViewAspect, UnityReferenceHeight)
		: FVector2D(UnityReferenceWidth, UnityReferenceWidth / ViewAspect);
	if (!DesiredDesignSize.Equals(CurrentExpandedDesignSize, 0.5f))
	{
		CurrentExpandedDesignSize = DesiredDesignSize;
		ExpandedDesignSizeBox->SetWidthOverride(CurrentExpandedDesignSize.X);
		ExpandedDesignSizeBox->SetHeightOverride(CurrentExpandedDesignSize.Y);
	}
}

UButton* URokUnityModalWidget::MakePlayerDataButton(const FName& Name, const FIntRect& IconRect, const FText& Label)
{
	UButton* Button = WidgetTree->ConstructWidget<URokUiAnimatedButton>(URokUiAnimatedButton::StaticClass(), Name);
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
	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("%sContent"), *Name.ToString()));
	Button->SetContent(Content);
	PlaceOnCanvas(Content, MakeAtlasImage(*FString::Printf(TEXT("%sIcon"), *Name.ToString()), SettingAtlasPath, IconRect, SettingAtlasSize, FVector2D(126.0f)), FVector2D(21.0f, 6.0f), FVector2D(126.0f));
	UTextBlock* LabelText = MakeModalText(*FString::Printf(TEXT("%sLabel"), *Name.ToString()), 20, FLinearColor(0.95f, 0.93f, 0.88f, 1.0f), true, ETextJustify::Center);
	LabelText->SetText(Label);
	PlaceOnCanvas(Content, LabelText, FVector2D(4.0f, 116.21f), FVector2D(160.0f, 22.0f), 2);
	return Button;
}

UButton* URokUnityModalWidget::MakeTextAtlasButton(
	const FName& Name,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize,
	const FText& Label,
	int32 FontSize,
	const FLinearColor& TextColor,
	const FText& SecondaryLabel)
{
	UButton* Button = WidgetTree->ConstructWidget<URokUiAnimatedButton>(URokUiAnimatedButton::StaticClass(), Name);
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
	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("%sContent"), *Name.ToString()));
	Button->SetContent(Content);
	PlaceOnCanvas(Content, MakeAtlasImage(*FString::Printf(TEXT("%sBackground"), *Name.ToString()), TexturePath, UnityRect, AtlasSize, DesiredSize), FVector2D::ZeroVector, DesiredSize);
	UTextBlock* LabelText = MakeModalText(*FString::Printf(TEXT("%sLabel"), *Name.ToString()), FontSize, TextColor, true, ETextJustify::Center);
	LabelText->SetText(Label);
	PlaceOnCanvas(Content, LabelText, SecondaryLabel.IsEmpty() ? FVector2D(0.0f, 10.0f) : FVector2D(0.0f, 5.0f), FVector2D(DesiredSize.X, SecondaryLabel.IsEmpty() ? DesiredSize.Y - 16.0f : 38.0f), 2);
	if (!SecondaryLabel.IsEmpty())
	{
		UTextBlock* SecondaryText = MakeModalText(*FString::Printf(TEXT("%sSecondary"), *Name.ToString()), 18, TextColor, true, ETextJustify::Center);
		SecondaryText->SetText(SecondaryLabel);
		PlaceOnCanvas(Content, SecondaryText, FVector2D(0.0f, 42.0f), FVector2D(DesiredSize.X, 28.0f), 2);
	}
	return Button;
}

UButton* URokUnityModalWidget::MakeAtlasButton(
	const FName& Name,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize)
{
	UButton* Button = WidgetTree->ConstructWidget<URokUiAnimatedButton>(URokUiAnimatedButton::StaticClass(), Name);
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
	Button->SetContent(MakeAtlasImage(*FString::Printf(TEXT("%sImage"), *Name.ToString()), TexturePath, UnityRect, AtlasSize, DesiredSize));
	return Button;
}

UButton* URokUnityModalWidget::MakeIndexedHitButton(
	const FName& Name,
	int32 ActionId)
{
	URokUnityIndexedButton* Button =
		WidgetTree->ConstructWidget<URokUnityIndexedButton>(
			URokUnityIndexedButton::StaticClass(),
			Name);
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
	Button->ConfigureAction(ActionId);
	Button->OnIndexedClicked.AddUObject(
		this,
		&URokUnityModalWidget::HandleProfileSubscreenAction);
	return Button;
}

UImage* URokUnityModalWidget::MakeAtlasImage(
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

UBorder* URokUnityModalWidget::MakeAtlasBorder(
	const FName& Name,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize,
	const FMargin& BrushMargin)
{
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	Border->SetPadding(FMargin(0.0f));
	Border->SetVisibility(ESlateVisibility::HitTestInvisible);
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	Border->SetBrush(NoBrush);
	Border->SetHorizontalAlignment(HAlign_Fill);
	Border->SetVerticalAlignment(VAlign_Fill);
	UCanvasPanel* Slices = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
		*FString::Printf(TEXT("%sSlices"), *Name.ToString()));
	Slices->SetVisibility(ESlateVisibility::HitTestInvisible);
	Border->SetContent(Slices);
	for (int32 Index = 0; Index < 9; ++Index)
	{
		const auto Patch = RokModalNineSlice::Patch(UnityRect, BrushMargin, DesiredSize, Index);
		if (!Patch.IsVisible()) continue;
		PlaceOnCanvas(Slices, MakeAtlasImage(*FString::Printf(TEXT("%sSlice_%d"), *Name.ToString(), Index),
			TexturePath, Patch.Source, AtlasSize, Patch.Size), Patch.Position, Patch.Size);
	}
	return Border;
}

UTextBlock* URokUnityModalWidget::MakeModalText(
	const FName& Name,
	int32 FontSize,
	const FLinearColor& Color,
	bool bBold,
	ETextJustify::Type Justification)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	Text->SetColorAndOpacity(FSlateColor(Color));
	Text->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), FontSize));
	Text->SetJustification(Justification);
	Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
	Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Text;
}

void URokUnityModalWidget::PlaceOnCanvas(
	UCanvasPanel* Parent,
	UWidget* Child,
	const FVector2D& Position,
	const FVector2D& Size,
	int32 ZOrder)
{
	check(Parent);
	check(Child);
	UCanvasPanelSlot* CanvasSlot = Parent->AddChildToCanvas(Child);
	CanvasSlot->SetPosition(Position);
	CanvasSlot->SetSize(Size);
	CanvasSlot->SetZOrder(ZOrder);
}

UTexture2D* URokUnityModalWidget::LoadUiTexture(const TCHAR* TexturePath) const
{
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok modal UI: missing Unity texture %s"), TexturePath);
	}
	return Texture;
}

FSlateBrush URokUnityModalWidget::MakeAtlasBrush(
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

URokBuildingUpgradeSubsystem* URokUnityModalWidget::GetUpgradeSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
}

void URokUnityModalWidget::HandleCloseClicked()
{
	RequestCloseModal();
}

void URokUnityModalWidget::HandleResourceTabFood()
{
	SelectedResourceCategory = 0;
	RebuildModeContent();
}

void URokUnityModalWidget::HandleResourceTabWood()
{
	SelectedResourceCategory = 1;
	RebuildModeContent();
}

void URokUnityModalWidget::HandleResourceTabStone()
{
	SelectedResourceCategory = 2;
	RebuildModeContent();
}

void URokUnityModalWidget::HandleResourceTabGold()
{
	SelectedResourceCategory = 3;
	RebuildModeContent();
}

void URokUnityModalWidget::HandlePreviewItemAction()
{
	UE_LOG(LogTemp, Log, TEXT("Rok modal UI: preview action selected; inventory/server command boundary is not migrated yet."));
}

void URokUnityModalWidget::HandleProfileArmyClicked()
{
	if (!FeatureScreenWidget)
	{
		return;
	}
	CloseModal();
	FeatureScreenWidget->OpenArmyStatistics();
}

void URokUnityModalWidget::HandleProfileAvatarClicked()
{
	OpenPlayerHeadSelection();
}

void URokUnityModalWidget::HandleProfileRenameClicked()
{
	OpenChangeName();
}

void URokUnityModalWidget::HandleChangeNameTextChanged(const FText& Text)
{
	if (bUpdatingChangeNameInput)
	{
		return;
	}

	const FString SourceText = Text.ToString();
	FString SanitizedText;
	SanitizedText.Reserve(SourceText.Len());
	for (const TCHAR Character : SourceText)
	{
		const uint32 CodeUnit = static_cast<uint32>(Character);
		const bool bFilteredByUnityRule =
			(CodeUnit >= 0x0800 && CodeUnit <= 0x9FA5)
			|| (CodeUnit >= 0xAC00 && CodeUnit <= 0xD7FF);
		if (!bFilteredByUnityRule)
		{
			SanitizedText.AppendChar(Character);
		}
	}
	if (SanitizedText.Len() > ChangeNameMaxLength)
	{
		int32 SafeLength = ChangeNameMaxLength;
		if (SafeLength < SanitizedText.Len()
			&& StringConv::IsHighSurrogate(
				static_cast<uint32>(SanitizedText[SafeLength - 1]))
			&& StringConv::IsLowSurrogate(
				static_cast<uint32>(SanitizedText[SafeLength])))
		{
			--SafeLength;
		}
		SanitizedText.LeftInline(
			SafeLength,
			EAllowShrinking::No);
	}

	ChangeNameDraft = SanitizedText;
	if (ChangeNameInput && SanitizedText != SourceText)
	{
		bUpdatingChangeNameInput = true;
		ChangeNameInput->SetText(FText::FromString(SanitizedText));
		bUpdatingChangeNameInput = false;
	}
	UpdateChangeNameValidationState();
}

void URokUnityModalWidget::HandleChangeNameConfirmClicked()
{
	const ERokChangeNameValidationResult Result = ValidateChangeNameCandidate(
		ChangeNameDraft,
		ChangeNameCurrentName,
		bChangeNameHasCurrentName,
		ChangeNameBannedWords,
		bChangeNameBannedWordsLoaded);
	UpdateChangeNameValidationState();
	if (Result != ERokChangeNameValidationResult::Valid)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok change-name UI rejected local candidate: result=%d length=%d; no profile state was mutated."),
			static_cast<int32>(Result),
			ChangeNameDraft.Len());
		return;
	}
	if (!bChangeNameHasCurrentName || !bChangeNameSubmitServiceAvailable)
	{
		if (ChangeNameValidationFeedback)
		{
			ChangeNameValidationFeedback->SetText(NSLOCTEXT(
				"RokModal",
				"ChangeNameSubmitBoundary",
				"改名服务未接入：没有发送请求、扣除物品或修改本地名称。"));
			ChangeNameValidationFeedback->SetColorAndOpacity(FSlateColor(
				FLinearColor(0.96f, 0.45f, 0.24f, 1.0f)));
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok change-name UI boundary: Role_ModifyName unavailable candidate_length=%d; no profile, inventory, or currency state was mutated."),
			ChangeNameDraft.Len());
		return;
	}
}

void URokUnityModalWidget::HandleChangeNameCancelClicked()
{
	RequestCloseModal();
}

void URokUnityModalWidget::HandleProfileCivilizationClicked()
{
	OpenCivilizationSelection();
}

void URokUnityModalWidget::HandleCivilizationContinueClicked()
{
	OpenCivilizationChangeConfirmation();
}

void URokUnityModalWidget::ClosePlayerPowerActionTip()
{
	if (PlayerPowerActionTipLayer) PlayerPowerActionTipLayer->RemoveFromParent();
	PlayerPowerActionTipLayer = nullptr;
	PlayerPowerActionTipRoot = nullptr;
	PlayerPowerActionTipTime = 0.f;
}

void URokUnityModalWidget::ClosePlayerKillTip()
{
	if (PlayerKillTipLayer) PlayerKillTipLayer->RemoveFromParent();
	PlayerKillTipLayer = nullptr;
	PlayerKillTipRoot = nullptr;
	PlayerKillTipTime = 0;
}

void URokUnityModalWidget::HandleProfileKillsHelpClicked()
{
	if (ActiveMode != ERokUnityModalMode::PlayerProfile || !ModeCanvas) return;
	// PlayerDataMediator.ShowKill -> HelpTip arrowUp; keep profile underneath.
	ClosePlayerKillTip();
	ClosePlayerPowerActionTip();
	UWidget* Anchor = GetWidgetFromName(TEXT("ProfileKillsHelpButton"));
	const auto* AnchorSlot = Anchor ? Cast<UCanvasPanelSlot>(Anchor->Slot) : nullptr;
	if (!AnchorSlot) return;
	const FVector2D AnchorCenter = AnchorSlot->GetPosition() + AnchorSlot->GetSize() * 0.5;
	using namespace RokPlayerKillTipAnimation;
	const float Left = FMath::Clamp(float(AnchorCenter.X - Width * 0.5), 0.0f,
		FMath::Max(0.0f, float(CurrentExpandedDesignSize.X) - Width));
	const float Top = AnchorCenter.Y + 12.0f;

	PlayerKillTipLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerKillTipLayer"));
	PlaceOnCanvas(ModeCanvas, PlayerKillTipLayer, FVector2D::ZeroVector, CurrentExpandedDesignSize, 100);
	auto* LayerSlot = CastChecked<UCanvasPanelSlot>(PlayerKillTipLayer->Slot);
	LayerSlot->SetAnchors(FAnchors(0, 0, 1, 1)); LayerSlot->SetOffsets(FMargin(0));
	FSlateBrush NoBrush; NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle EmptyStyle;
	EmptyStyle.SetNormal(NoBrush); EmptyStyle.SetHovered(NoBrush); EmptyStyle.SetPressed(NoBrush);
	EmptyStyle.SetDisabled(NoBrush); EmptyStyle.SetNormalPadding(FMargin(0)); EmptyStyle.SetPressedPadding(FMargin(0));
	UButton* Mask = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayerKillTipClickAway"));
	Mask->SetStyle(EmptyStyle); Mask->OnClicked.AddDynamic(this, &URokUnityModalWidget::ClosePlayerKillTip);
	PlaceOnCanvas(PlayerKillTipLayer, Mask, FVector2D::ZeroVector, CurrentExpandedDesignSize);
	auto* MaskSlot = CastChecked<UCanvasPanelSlot>(Mask->Slot);
	MaskSlot->SetAnchors(FAnchors(0, 0, 1, 1)); MaskSlot->SetOffsets(FMargin(0));
	PlayerKillTipRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerKillTipRoot"));
	PlaceOnCanvas(PlayerKillTipLayer, PlayerKillTipRoot, FVector2D(Left, Top), FVector2D(Width, Height), 1);
	PlayerKillTipRoot->SetRenderTransformPivot(FVector2D(0.5));
	PlayerKillTipRoot->SetRenderScale(FVector2D(0));
	// A hit area inside the bubble prevents a click on content closing HelpTip.
	UButton* Inside = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayerKillTipInside"));
	Inside->SetStyle(EmptyStyle);
	PlaceOnCanvas(PlayerKillTipRoot, Inside, FVector2D::ZeroVector, FVector2D(Width, Height));
	PlaceOnCanvas(PlayerKillTipRoot, MakeAtlasBorder(TEXT("PlayerKillTipBackground"), CommonAtlasPath,
		SpriteRect(774, 276, 72, 72), CommonAtlasSize, FVector2D(Width, Height),
		FMargin(21.0f / 72, 23.0f / 72, 21.0f / 72, 24.0f / 72)), FVector2D::ZeroVector, FVector2D(Width, Height), 1);
	UImage* Arrow = MakeAtlasImage(TEXT("PlayerKillTipArrow"), CommonAtlasPath, SpriteRect(120, 137, 15, 12), CommonAtlasSize, FVector2D(15, 12));
	Arrow->SetRenderTransformPivot(FVector2D(0.5)); Arrow->SetRenderTransformAngle(180);
	PlaceOnCanvas(PlayerKillTipRoot, Arrow, FVector2D(AnchorCenter.X - Left - 7.5f, -10.6f), FVector2D(15, 12), 2);
	UTextBlock* Total = MakeModalText(TEXT("PlayerKillTipTotal"), 18, FLinearColor(0.137f, 0.137f, 0.125f, 1), true);
	Total->SetText(NSLOCTEXT("RokModal", "PlayerKillTotalUnknown", "击杀总数：—（待同步）"));
	PlaceOnCanvas(PlayerKillTipRoot, Total, FVector2D(37.37f, 16.34f), FVector2D(280.58f, 30), 3);
	const FIntRect TierRects[] = { SpriteRect(1, 11, 23, 24), SpriteRect(1, 37, 24, 24),
		SpriteRect(461, 999, 24, 24), SpriteRect(53, 115, 24, 24), SpriteRect(1, 63, 24, 24) };
	// Saved upper-right becomes upper-left in Chinese: ArabLayoutCompment.Awake
	// flips GridLayoutGroup for flags=512/Preview=false. Two 144x24 columns,
	// spacing Y=28. The creation-date label is inactive in the source prefab.
	for (int32 Tier = 0; Tier < 5; ++Tier)
	{
		const FVector2D Cell(43.0f + (Tier % 2) * 144.0f, 59.95f + (Tier / 2) * 52.0f);
		PlaceOnCanvas(PlayerKillTipRoot, MakeAtlasImage(*FString::Printf(TEXT("PlayerKillTipTierIcon_%d"), Tier + 1),
			PlayerUiAtlasPath, TierRects[Tier], PlayerUiAtlasSize, FVector2D(23, 24)), Cell, FVector2D(23, 24), 3);
		UTextBlock* Count = MakeModalText(*FString::Printf(TEXT("PlayerKillTipTierCount_%d"), Tier + 1), 18, FLinearColor(0.137f, 0.137f, 0.125f, 1));
		Count->SetText(NSLOCTEXT("RokModal", "PlayerKillTierUnknown", "—"));
		PlaceOnCanvas(PlayerKillTipRoot, Count, Cell + FVector2D(35.9f, -3), FVector2D(105, 30), 3);
	}
	UE_LOG(LogTemp, Display, TEXT("Rok player kill tooltip opened: source=UI_KillTip statistics=server_unknown"));
}

void URokUnityModalWidget::HandleProfileActionHelpClicked()
{
	if (ActiveMode != ERokUnityModalMode::PlayerProfile || !ModeCanvas) return;
	// PlayerDataMediator.ShowPowerAction -> HelpTip arrowUp; keep profile underneath.
	ClosePlayerPowerActionTip();
	ClosePlayerKillTip();
	UWidget* Anchor = GetWidgetFromName(TEXT("ProfileActionHelpButton"));
	const auto* AnchorSlot = Anchor ? Cast<UCanvasPanelSlot>(Anchor->Slot) : nullptr;
	if (!AnchorSlot) return;
	const FVector2D AnchorCenter = AnchorSlot->GetPosition() + AnchorSlot->GetSize() * 0.5;
	using namespace RokPowerActionTip;
	const float Left = FMath::Clamp(float(AnchorCenter.X - Width * 0.5), 0.0f,
		FMath::Max(0.0f, float(CurrentExpandedDesignSize.X) - Width));
	const float Top = AnchorCenter.Y + 12.0f;

	PlayerPowerActionTipLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerActionTipLayer"));
	PlaceOnCanvas(ModeCanvas, PlayerPowerActionTipLayer, FVector2D::ZeroVector, CurrentExpandedDesignSize, 100);
	auto* LayerSlot = CastChecked<UCanvasPanelSlot>(PlayerPowerActionTipLayer->Slot);
	LayerSlot->SetAnchors(FAnchors(0, 0, 1, 1)); LayerSlot->SetOffsets(FMargin(0));
	FSlateBrush NoBrush; NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle EmptyStyle;
	EmptyStyle.SetNormal(NoBrush); EmptyStyle.SetHovered(NoBrush); EmptyStyle.SetPressed(NoBrush);
	EmptyStyle.SetDisabled(NoBrush); EmptyStyle.SetNormalPadding(FMargin(0)); EmptyStyle.SetPressedPadding(FMargin(0));
	UButton* Mask = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayerActionTipClickAway"));
	Mask->SetStyle(EmptyStyle); Mask->OnClicked.AddDynamic(this, &URokUnityModalWidget::ClosePlayerPowerActionTip);
	PlaceOnCanvas(PlayerPowerActionTipLayer, Mask, FVector2D::ZeroVector, CurrentExpandedDesignSize);
	auto* MaskSlot = CastChecked<UCanvasPanelSlot>(Mask->Slot);
	MaskSlot->SetAnchors(FAnchors(0, 0, 1, 1)); MaskSlot->SetOffsets(FMargin(0));
	PlayerPowerActionTipRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerActionTipRoot"));
	PlaceOnCanvas(PlayerPowerActionTipLayer, PlayerPowerActionTipRoot, FVector2D(Left, Top), FVector2D(Width, Height), 1);
	PlayerPowerActionTipRoot->SetRenderTransformPivot(FVector2D(0.5));
	PlayerPowerActionTipRoot->SetRenderScale(FVector2D(0));
	// A hit area inside the bubble prevents a click on content closing HelpTip.
	UButton* Inside = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayerActionTipInside"));
	Inside->SetStyle(EmptyStyle);
	PlaceOnCanvas(PlayerPowerActionTipRoot, Inside, FVector2D::ZeroVector, FVector2D(Width, Height));
	PlaceOnCanvas(PlayerPowerActionTipRoot, MakeAtlasBorder(TEXT("PlayerActionTipBackground"), CommonAtlasPath,
		SpriteRect(774, 276, 72, 72), CommonAtlasSize, FVector2D(Width, Height),
		FMargin(21.0f / 72, 23.0f / 72, 21.0f / 72, 24.0f / 72)), FVector2D::ZeroVector, FVector2D(Width, Height), 1);
	UImage* Arrow = MakeAtlasImage(TEXT("PlayerActionTipArrow"), CommonAtlasPath, SpriteRect(120, 137, 15, 12), CommonAtlasSize, FVector2D(15, 12));
	Arrow->SetRenderTransformPivot(FVector2D(0.5)); Arrow->SetRenderTransformAngle(180);
	PlaceOnCanvas(PlayerPowerActionTipRoot, Arrow, FVector2D(AnchorCenter.X - Left - 7.5f, -10.6f), FVector2D(15, 12), 2);
	// Unity Text uses MiddleCenter; zero-padding border supplies vertical centering.
	auto PlaceTextRegion = [this, &NoBrush](UWidget* Text, const FName& RegionName, float Y, float RegionHeight)
	{
		auto* Region = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), RegionName);
		Region->SetBrush(NoBrush); Region->SetPadding(FMargin(0));
		Region->SetHorizontalAlignment(HAlign_Fill); Region->SetVerticalAlignment(VAlign_Center);
		Region->SetVisibility(ESlateVisibility::HitTestInvisible); Region->SetContent(Text);
		PlaceOnCanvas(PlayerPowerActionTipRoot, Region, FVector2D(20, Y), FVector2D(BodyWidth, RegionHeight), 3);
	};
	auto MakeBodyText = [this](const FName& Name, const FText& Content)
	{
		auto* Text = MakeModalText(Name, 12, FLinearColor::Black, false, ETextJustify::Center);
		auto Font = Text->GetFont(); Font.Size = RokPowerActionTip::FontPoints(16); Text->SetFont(Font);
		Text->SetShadowOffset(FVector2D::ZeroVector); Text->SetShadowColorAndOpacity(FLinearColor::Transparent);
		Text->SetAutoWrapText(true); Text->SetWrapTextAt(RokPowerActionTip::BodyWidth); Text->SetText(Content);
		return Text;
	};
	PlaceTextRegion(MakeBodyText(TEXT("PlayerActionTipDescription"),
		NSLOCTEXT("RokModal", "PowerActionTipDescription", "行动力能够用于攻打乱军，并会随时间缓慢恢复。")),
		TEXT("PlayerActionTipDescriptionRegion"), DescriptionY, DescriptionHeight);
	PlaceTextRegion(MakeBodyText(TEXT("PlayerActionTipWarning"),
		NSLOCTEXT("RokModal", "PowerActionTipWarning", "（现有行动力超过上限时，取消行动将不会返还任何行动力）")),
		TEXT("PlayerActionTipWarningRegion"), WarningY, WarningHeight);
	auto* Recovery = WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass(), TEXT("PlayerActionTipRecovery"));
	FTextBlockStyle RecoveryStyle;
	auto RecoveryFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14);
	RecoveryFont.Size = FontPoints(18); // 18 Unity px = 13.5 Slate pt at 96 DPI.
	RecoveryStyle.SetFont(RecoveryFont).SetColorAndOpacity(FSlateColor(FLinearColor::Black));
	RecoveryStyle.SetShadowOffset(FVector2D::ZeroVector);
	auto* Styles = NewObject<UDataTable>(Recovery); Styles->RowStruct = FRichTextStyleRow::StaticStruct();
	FRichTextStyleRow DefaultRow; DefaultRow.TextStyle = RecoveryStyle;
	Styles->AddRow(TEXT("Default"), DefaultRow);
	FRichTextStyleRow RemainingRow; RemainingRow.TextStyle = RecoveryStyle;
	RemainingRow.TextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(0.f, 128.f / 255.f, 0.f, 1.f)));
	Styles->AddRow(TEXT("Remaining"), RemainingRow);
	Recovery->SetTextStyleSet(Styles); Recovery->SetDefaultTextStyle(RecoveryStyle);
	Recovery->SetJustification(ETextJustify::Center); Recovery->SetAutoWrapText(true); Recovery->SetWrapTextAt(BodyWidth);
	Recovery->SetVisibility(ESlateVisibility::HitTestInvisible);
	// Source 100708 needs city attributes, lastActionForceTime and server time.
	// Keep every unavailable term unknown; there is no synthetic recovery timer.
	Recovery->SetText(NSLOCTEXT("RokModal", "PowerActionTipRecoveryUnknown",
		"行动力恢复1点需要—秒\n（恢复速度提升：+ —%）\n\n全部恢复还需   <Remaining>—</>"));
	PlaceTextRegion(Recovery, TEXT("PlayerActionTipRecoveryRegion"), RecoveryY, RecoveryHeight);
	UE_LOG(LogTemp, Display, TEXT("Rok power-action tooltip opened: source=UI_Pop_PowerTextTip recovery=server_unknown"));
}

void URokUnityModalWidget::HandleProfileActionAddClicked()
{
	ShowProfileBoundaryFeedback(
		NSLOCTEXT(
			"RokModal",
			"ProfileActionAddBoundary",
			"补充行动力需要库存消费与服务器确认，当前不会扣除或增加数值。"),
		TEXT("action_add"));
}

void URokUnityModalWidget::HandleProfileMoreClicked()
{
	OpenPlayerPowerDetails();
}

void URokUnityModalWidget::HandleProfileRankingClicked()
{
	OpenPlayerRanking();
}

void URokUnityModalWidget::HandleProfileSettingsClicked()
{
	if (!FeatureScreenWidget)
	{
		return;
	}
	CloseModal();
	FeatureScreenWidget->OpenSettingsHub();
}

void URokUnityModalWidget::HandleProfileSubscreenAction(int32 ActionId)
{
	if (ActiveMode == ERokUnityModalMode::PlayerHeadSelection)
	{
		if (ActionId >= 3000 && PlayerHeadPageRows.IsValidIndex(ActionId - 3000))
		{
			SelectedPlayerHeadIndex = ActionId - 3000;
			PlayerHeadScrollOffset = PlayerHeadGridScroll ? PlayerHeadGridScroll->GetScrollOffset() : 0.0f;
			RebuildModeContent();
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok player-head UI selection: page=%d index=%d state_unchanged=1 authority=server."),
				SelectedPlayerHeadPage,
				SelectedPlayerHeadIndex);
			return;
		}
		if (ActionId >= 3100 && ActionId <= 3101)
		{
			if (SelectedPlayerHeadPage == ActionId - 3100) return;
			SelectedPlayerHeadPage = ActionId - 3100;
			SelectedPlayerHeadIndex = INDEX_NONE;
			PlayerHeadScrollOffset = 0.0f;
			RebuildModeContent();
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok player-head UI page changed: page=%d state_unchanged=1 authority=server."),
				SelectedPlayerHeadPage);
			return;
		}
	}

	if (ActiveMode == ERokUnityModalMode::CivilizationSelection
		&& ActionId >= 3300 && ActionId < 3307)
	{
		TArray<FRokUnityCatalogCivilization> Civilizations =
			LoadCivilizationRowsForUi(GetGameInstance());
		Civilizations.Sort(
			[](const FRokUnityCatalogCivilization& Left,
				const FRokUnityCatalogCivilization& Right)
			{
				return Left.bLocked == Right.bLocked
					? Left.Id < Right.Id
					: !Left.bLocked;
			});
		const int32 Index = ActionId - 3300;
		if (Civilizations.IsValidIndex(Index) && !Civilizations[Index].bLocked)
		{
			SelectedCivilizationId = Civilizations[Index].Id;
			bCivilizationSwitchAnimation = true;
			CivilizationSelectionAnimationTime = 0;
			CivilizationHeroAnimationTime = 0;
			RebuildModeContent();
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok civilization selection changed: civilization=%d state_unchanged=1 authority=server."),
				SelectedCivilizationId);
		}
		return;
	}

	if (ActiveMode == ERokUnityModalMode::PlayerRanking)
	{
		if (ActionId >= 3200 && PlayerRankingRows.IsValidIndex(ActionId - 3200))
		{
			SelectedRankingCategory = ActionId - 3200;
			RebuildModeContent();
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok player-ranking UI category selected: category=%d query=unavailable state_unchanged=1 authority=server."),
				SelectedRankingCategory);
			return;
		}
		if (ActionId == 3299)
		{
			SelectedRankingCategory = INDEX_NONE;
			RebuildModeContent();
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok player-ranking UI returned to category grid."));
		}
	}
}

void URokUnityModalWidget::HandleBuffListMoreClicked()
{
	if (!FeatureScreenWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok buff-list UI: city-manager screen is unavailable."));
		return;
	}
	CloseModal();
	FeatureScreenWidget->OpenCityManager();
}

void URokUnityModalWidget::ShowProfileBoundaryFeedback(
	const FText& Message,
	const TCHAR* LogAction)
{
	if (FeedbackText)
	{
		FeedbackText->SetText(Message);
		FeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok profile UI boundary: %s."),
		LogAction);
}

void URokUnityModalWidget::HandleStartUpgrade()
{
	URokBuildingUpgradeSubsystem* Subsystem = GetUpgradeSubsystem();
	if (!Subsystem || !SelectedBuildingActor.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("Rok modal UI: source preview did not submit a timed upgrade."));
		if (FeedbackText)
		{
			FeedbackText->SetText(NSLOCTEXT("RokModal", "SourcePreviewNoMutation", "配置预览，不会写入玩家状态"));
			FeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		return;
	}
	FString FailureReason;
	if (Subsystem->StartUpgrade(SelectedBuildingActor.Get(), FailureReason))
	{
		UE_LOG(LogTemp, Log, TEXT("Rok modal UI: timed building upgrade started."));
		CloseModal();
		return;
	}
	if (FeedbackText)
	{
		FeedbackText->SetText(FText::FromString(FailureReason));
		FeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void URokUnityModalWidget::HandleInstantUpgradePreview()
{
	UE_LOG(LogTemp, Log, TEXT("Rok modal UI: immediate upgrade requires migrated Gem/Denar state; no currency was spent."));
	if (FeedbackText)
	{
		FeedbackText->SetText(NSLOCTEXT("RokModal", "GemStateNotMigrated", "钻石状态尚未迁移，本次未扣除货币"));
		FeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
