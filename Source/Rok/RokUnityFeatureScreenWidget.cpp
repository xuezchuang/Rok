#include "RokUnityFeatureScreenWidget.h"
#include "RokAllianceTerritoryReadonlyData.h"
#include "RokAllianceRankingReadonlyCatalog.h"
#include "RokHospitalExplanationWidget.h"
#include "RokTroopPortrait.h"
#include "RokPlayerController.h"
#include "RokResearchScreenWidget.h"
#include "RokResearchCatalog.h"
#include "RokHelpWidget.h"
#include "RokMailAttachmentMeshWidget.h"
#include "RokCommanderRosterAnimation.h"
#include "RokCommanderDataAnimation.h"
#include "RokDataPaths.h"
#include "ImageUtils.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/App.h"
#include "RokCommanderSkillReadonlyDetail.h"
#include "RokArmyShowPanelAnimation.h"
#include "RokTrainArmyDetail.h"
#include "RokTrainArmyMeshWidget.h"
#include "RokTrainArmyQuantity.h"
#include "RokTrainResourceCost.h"
#include "RokTrainParameterLayout.h"
#include "RokGraphicsSettingsLayout.h"
#include "RokLanguageSettingsLayout.h"
#include "RokUiSpec.h"
#include "RokUiWidgets.h"
#include "Components/RichTextBlock.h"
#include "Engine/DataTable.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "RokBuildCityCardAnimation.h"
#include "RokBuildingSourcePreviewData.h"
#include "Components/SizeBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "TimerManager.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fonts/CompositeFont.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokServerCitySubsystem.h"
#include "RokServerTaskSubsystem.h"
#include "RokServerArmySubsystem.h"
#include "RokUiTextTable.h"
#include "RokServerInventorySubsystem.h"
#include "RokGameNetSubsystem.h"
#include "RokLocalConstructionVisualFactory.h"
#include "RokClientSettingsSubsystem.h"
#include "RokAudioRouterSubsystem.h"
#include "RokCommanderMeshWidget.h"
#include "RokMainHudWidget.h"
#include "RokStrategyCameraPawn.h"
#include "RokUnityEconomyCatalogSubsystem.h"
#include "RokUnityConfigSubsystem.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapConfigActor.h"
#include "RokWorldMapPlayerController.h"
#include "Algo/Count.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"

namespace
{
int32 ReadLocalUiPreviewOption(const TCHAR* Option, int32 Minimum, int32 Maximum)
{
#if !UE_BUILD_SHIPPING
	int32 Value = INDEX_NONE;
	if (FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& FParse::Value(FCommandLine::Get(), Option, Value)
		&& Value >= Minimum && Value <= Maximum)
	{
		return Value;
	}
#endif
	return INDEX_NONE;
}

int32 GetTrainArmyDetailPreviewTier()
{
	return ReadLocalUiPreviewOption(TEXT("RokTrainArmyPreviewDetail="), 1, 5);
}

int32 GetBuildingSpeedUpPreview()
{
	return ReadLocalUiPreviewOption(TEXT("RokSpeedUpServerPreview="), 1, 3);
}

enum class EHospitalLocalPreview : int32
{
	None = INDEX_NONE,
	Empty = 1,
	Wounded,
	Treating,
	Ready,
	Confirm,
};

constexpr int64 HospitalPreviewFinishTime = 1280;

EHospitalLocalPreview GetHospitalPreviewState()
{
	return static_cast<EHospitalLocalPreview>(ReadLocalUiPreviewOption(TEXT("RokHospitalPreviewState="),
		static_cast<int32>(EHospitalLocalPreview::Empty), static_cast<int32>(EHospitalLocalPreview::Confirm)));
}

TSharedPtr<FJsonObject> ReadHospitalDisplayRole(const URokGameNetSubsystem* Net, EHospitalLocalPreview Preview)
{
#if !UE_BUILD_SHIPPING
	if (Preview != EHospitalLocalPreview::None)
	{
		const auto Role = MakeShared<FJsonObject>();
		Role->SetNumberField(TEXT("country"), 301);
		Role->SetNumberField(TEXT("food"), 1000);
		Role->SetNumberField(TEXT("wood"), 1000);
		Role->SetNumberField(TEXT("denar"), 20);
		if (Preview != EHospitalLocalPreview::Empty)
		{
			const auto MakeSoldier = [](int32 Id, int32 Type, int32 Count)
			{
				const auto Soldier = MakeShared<FJsonObject>();
				Soldier->SetNumberField(TEXT("id"), Id);
				Soldier->SetNumberField(TEXT("type"), Type);
				Soldier->SetNumberField(TEXT("level"), 1);
				Soldier->SetNumberField(TEXT("num"), Count);
				return MakeShared<FJsonValueObject>(Soldier);
			};
			const TArray<TSharedPtr<FJsonValue>> Wounded = {
				MakeSoldier(10101, 1, 4), MakeSoldier(10301, 3, 13)};
			Role->SetArrayField(TEXT("seriousInjured"), Wounded);
			if (Preview == EHospitalLocalPreview::Treating || Preview == EHospitalLocalPreview::Ready)
			{
				const auto Queue = MakeShared<FJsonObject>();
				Queue->SetNumberField(TEXT("queueIndex"), 1);
				Queue->SetNumberField(TEXT("finishTime"), HospitalPreviewFinishTime);
				Queue->SetArrayField(TEXT("treatmentSoldiers"), Wounded);
				Role->SetObjectField(TEXT("treatmentQueue"), Queue);
			}
		}
		return Role;
	}
#endif
	return Net && Net->GetState() == ERokGameNetState::RoleReady ? Net->GetRoleInfo() : nullptr;
}

bool ParseWorldPositionInt32Strict(const FString& Text, int32& OutValue)
{
	if (Text.IsEmpty())
	{
		return false;
	}
	int32 Index = 0;
	bool bNegative = false;
	if (Text[0] == TEXT('+') || Text[0] == TEXT('-'))
	{
		bNegative = Text[0] == TEXT('-');
		Index = 1;
		if (Index == Text.Len())
		{
			return false;
		}
	}
	const int64 Limit = bNegative ? 2147483648LL : 2147483647LL;
	int64 Magnitude = 0;
	for (; Index < Text.Len(); ++Index)
	{
		const TCHAR Character = Text[Index];
		if (Character < TEXT('0') || Character > TEXT('9'))
		{
			return false;
		}
		const int64 Digit = Character - TEXT('0');
		if (Magnitude > (Limit - Digit) / 10)
		{
			return false;
		}
		Magnitude = Magnitude * 10 + Digit;
	}
	OutValue = static_cast<int32>(bNegative ? -Magnitude : Magnitude);
	return true;
}

bool ResolveWorldPositionDisplayBounds(UWorld* World, FIntPoint& OutMinimum,
	FIntPoint& OutMaximum, bool& bOutLocalFallback)
{
	FRokWorldMapProjection Projection;
	bOutLocalFallback = true;
	if (World)
	{
		int32 ConfigCount = 0;
		for (TActorIterator<ARokWorldMapConfigActor> It(World); It; ++It)
		{
			if (++ConfigCount > 1)
			{
				return false;
			}
			Projection = It->Projection;
			bOutLocalFallback = false;
		}
	}
	if (!Projection.IsValid()
		|| !FMath::IsFinite(Projection.UnityBoundsMin.X)
		|| !FMath::IsFinite(Projection.UnityBoundsMin.Y)
		|| !FMath::IsFinite(Projection.UnityBoundsMax.X)
		|| !FMath::IsFinite(Projection.UnityBoundsMax.Y))
	{
		return false;
	}
	const double Scale = Projection.DisplayCoordinateScale;
	const FVector2D DisplayMinimum = Projection.UnityBoundsMin / Scale;
	const FVector2D DisplayMaximum = Projection.UnityBoundsMax / Scale;
	if (DisplayMinimum.X < MIN_int32 || DisplayMinimum.Y < MIN_int32
		|| DisplayMaximum.X > MAX_int32 || DisplayMaximum.Y > MAX_int32)
	{
		return false;
	}
	OutMinimum = FIntPoint(FMath::CeilToInt(DisplayMinimum.X),
		FMath::CeilToInt(DisplayMinimum.Y));
	OutMaximum = FIntPoint(FMath::FloorToInt(DisplayMaximum.X),
		FMath::FloorToInt(DisplayMaximum.Y));
	return OutMinimum.X <= OutMaximum.X && OutMinimum.Y <= OutMaximum.Y;
}

FSlateFontInfo MailSourceQuantityFont()
{
	// Unity's built-in font 10102 is Arial. Reward quantities contain only
	// digits and punctuation, so the host Arial Bold face needs no CJK fallback.
	const FString WindowsDirectory = FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR"));
	const FString ArialBoldPath = FPaths::Combine(WindowsDirectory, TEXT("Fonts/arialbd.ttf"));
	const float PointSize = 26.0f * 0.7f * 72.0f / 96.0f;
	if (IFileManager::Get().FileExists(*ArialBoldPath))
	{
		// Same single-face composite and lazy loading as the UE5.8 legacy
		// filename constructor, using its supported FCompositeFont API.
		static TSharedPtr<const FCompositeFont> ArialBoldFont =
			MakeShared<FStandaloneCompositeFont>(NAME_None, ArialBoldPath,
				EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
		return FSlateFontInfo(ArialBoldFont, PointSize);
	}
	return FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), PointSize);
}

const TCHAR* CommonAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
const TCHAR* FrameAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame");
const TCHAR* PageAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_page.rok_modal_atlas_page");
const TCHAR* SettingAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_setting.rok_modal_atlas_setting");
const TCHAR* Item0AtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item0.rok_modal_atlas_item0");
const TCHAR* Item1AtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item1.rok_modal_atlas_item1");
const TCHAR* ActivityImageAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_activity_img.rok_fullscreen_activity_img");
const TCHAR* ActivityIconAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_activity_icon.rok_fullscreen_activity_icon");
const TCHAR* MailAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_mail.rok_fullscreen_mail");
const TCHAR* TaskIconAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_task_icon.rok_fullscreen_task_icon");
const TCHAR* Item2AtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item2.rok_fullscreen_item2");
const TCHAR* Item3AtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item3.rok_fullscreen_item3");
const TCHAR* Item4AtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item4.rok_fullscreen_item4");
const TCHAR* Item5AtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5.rok_fullscreen_item5");
const TCHAR* TreasureBoxAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_treasure_box.rok_fullscreen_treasure_box");
const TCHAR* AllianceAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance.rok_fullscreen_alliance");
const TCHAR* AllianceFlagAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_alliance_flag.rok_fullscreen_alliance_flag");
const TCHAR* VipPlayerAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_player.rok_fullscreen_player");
const TCHAR* ChatAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_chat.rok_fullscreen_chat");
const TCHAR* EmojiAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_emoji.rok_fullscreen_emoji");
const TCHAR* VipLevelAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_vip.rok_hud_atlas_vip");
const TCHAR* CommanderHeadAtlasPath = TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_head.rok_commander_head");
const TCHAR* CommanderUiAtlasPath = TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_ui.rok_commander_ui");
const TCHAR* CommanderSkillAtlasPath = TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_skill.rok_commander_skill");
const TCHAR* CommanderSkill2AtlasPath = TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_skill2.rok_commander_skill2");
const TCHAR* TroopHeadAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_troops_head.rok_fullscreen_troops_head");
const TCHAR* WorkerMatrixAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_worker_matrix_icon.rok_fullscreen_worker_matrix_icon");
const TCHAR* BuildCityImageAtlasPath = TEXT("/Game/RokPrototype/UI/BuildCityAtlases/rok_buildcity_atlas_buildingimg.rok_buildcity_atlas_buildingimg");
const TCHAR* BuildCityLoadingAtlasPath = TEXT("/Game/RokPrototype/UI/BuildCityAtlases/rok_buildcity_atlas_login_loading.rok_buildcity_atlas_login_loading");
const TCHAR* ResourceSearchAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_map_icon.rok_hud_atlas_map_icon");

const FIntPoint CommonAtlasSize(2048, 1024);
const FIntPoint FrameAtlasSize(1024, 2048);
const FIntPoint PageAtlasSize(128, 2048);
const FIntPoint SettingAtlasSize(1024, 1024);
const FIntPoint Item0AtlasSize(512, 512);
const FIntPoint Item1AtlasSize(256, 1024);
const FIntPoint ActivityImageAtlasSize(2048, 2048);
const FIntPoint ActivityIconAtlasSize(1024, 256);
const FIntPoint MailAtlasSize(2048, 1024);
const FIntPoint TaskIconAtlasSize(256, 1024);
const FIntPoint Item2AtlasSize(256, 512);
const FIntPoint Item3AtlasSize(256, 1024);
const FIntPoint Item4AtlasSize(2048, 2048);
const FIntPoint Item5AtlasSize(1024, 2048);
const FIntPoint TreasureBoxAtlasSize(1263, 380);
const FIntPoint AllianceAtlasSize(1024, 2048);
const FIntPoint AllianceFlagAtlasSize(1024, 2048);
const FIntPoint VipPlayerAtlasSize(512, 1024);
const FIntPoint ChatAtlasSize(1024, 128);
const FIntPoint EmojiAtlasSize(256, 1024);
const FIntPoint VipLevelAtlasSize(512, 1024);
const FIntPoint CommanderHeadAtlasSize(1024, 2048);
const FIntPoint CommanderUiAtlasSize(2048, 512);
const FIntPoint CommanderSkillAtlasSize(2048, 2048);
const FIntPoint TroopHeadAtlasSize(1024, 1024);
const FIntPoint WorkerMatrixAtlasSize(512, 2048);
const FIntPoint BuildCityImageAtlasSize(512, 1024);
const FIntPoint BuildCityLoadingAtlasSize(1024, 2048);
const FIntPoint ResourceSearchAtlasSize(1024, 2048);

FIntRect SpriteRect(int32 X, int32 Y, int32 Width, int32 Height)
{
	return FIntRect(X, Y, X + Width, Y + Height);
}

// Unity activity_icon.png.meta sprites already reside in the imported 1024x256 atlas.
// Unknown names intentionally use the existing neutral common-panel fallback.
bool ResolveActivityCalendarIcon(const FString& Reference, FIntRect& OutRect)
{
	struct FSourceSprite { int32 Id; int32 X; int32 Y; int32 W; int32 H; };
	static constexpr FSourceSprite Sprites[] = {
		{1000,497,195,60,60}, {1001,431,9,60,60}, {1002,369,9,60,60},
		{1003,431,71,60,60}, {1004,435,133,60,60}, {1005,307,9,60,60},
		{1006,369,71,60,60}, {1007,245,9,60,60}, {1008,307,71,60,60},
		{1009,1,1,56,66}, {1010,373,133,60,60}, {1011,435,195,60,60},
		{1012,311,133,60,60}, {1013,307,9,60,60}, {1014,373,195,60,60},
		{1015,245,71,60,60}, {1016,249,133,60,60}, {1017,311,195,60,60},
		{1018,249,195,60,60}, {1019,183,9,60,60}, {1020,435,133,60,60},
		{1021,183,71,60,60}, {1022,121,39,60,60}, {1023,187,133,60,60},
		{1024,59,7,60,60}, {1025,94,101,60,60}, {1026,187,195,60,60},
		{1027,187,195,60,60}, {1028,187,195,60,60},
		{2000,94,163,91,92}, {2001,1,69,91,92}, {2002,1,163,91,92}
	};
	for (const FSourceSprite& Sprite : Sprites)
	{
		if (Reference == FString::Printf(TEXT("activity_icon[activity_icon_%d]"), Sprite.Id))
		{
			OutRect = SpriteRect(Sprite.X, Sprite.Y, Sprite.W, Sprite.H);
			return true;
		}
	}
	return false;
}

FVector2D CapturePoint(float X, float Y)
{
	// The references were captured with Unity Expand at 1440x900. The actual
	// prefab canvas is 1440x810 and is centred inside that expanded height.
	return FVector2D(X, Y - 45.0f);
}

const FIntRect WindowBodyRect = SpriteRect(1, 822, 524, 659);
const FIntRect WindowHeaderRect = SpriteRect(1, 329, 560, 60);
const FIntRect ActivityFrameRect = SpriteRect(527, 822, 118, 392);
const FIntRect CloseRect = SpriteRect(883, 589, 69, 69);
const FIntRect CommonPanelRect = SpriteRect(436, 165, 63, 55);
const FIntRect TaskRowRect = SpriteRect(1952, 906, 74, 67);
const FIntRect SideNormalRect = SpriteRect(1111, 387, 41, 100);
const FIntRect SideSelectedRect = SpriteRect(474, 426, 61, 100);
const FIntRect GroupLineRect = SpriteRect(1, 611, 570, 2);
const FIntRect ArmyShowRuleBarRect = SpriteRect(1003, 976, 964, 47);
const FIntRect ArmyShowRuleIconRect = SpriteRect(1062, 489, 21, 21);
const FIntRect CityManagerBackRect = SpriteRect(703, 282, 69, 69);
const FIntRect CityManagerIconFrameRect = SpriteRect(1741, 453, 84, 84);
const FIntRect CityManagerArrowRect = SpriteRect(667, 403, 28, 17);
const FIntRect SettingCharacterIconRect = SpriteRect(257, 85, 126, 126);
const FIntRect SettingNotificationIconRect = SpriteRect(257, 213, 126, 126);
const FIntRect SettingLanguageIconRect = SpriteRect(257, 341, 126, 126);
const FIntRect SettingCommunityIconRect = SpriteRect(257, 469, 126, 126);
const FIntRect SettingHelpIconRect = SpriteRect(129, 85, 126, 126);
const FIntRect SettingGeneralIconRect = SpriteRect(129, 341, 126, 126);
const FIntRect SettingAccountIconRect = SpriteRect(670, 586, 126, 126);
const FIntRect SettingMusicIconRect = SpriteRect(864, 842, 46, 45);
const FIntRect SettingSfxIconRect = SpriteRect(362, 607, 41, 45);
const FIntRect SettingSliderPlateRect = SpriteRect(1, 970, 883, 53);
const FIntRect SettingSwitchKnobRect = SpriteRect(197, 7, 43, 43);
const FIntRect SettingSwitchOffRect = SpriteRect(341, 137, 82, 40);
const FIntRect SettingSwitchOnRect = SpriteRect(674, 699, 82, 40);
const FIntRect LoginDeviceIconRect = SpriteRect(645, 290, 48, 48);
const FIntRect LoginIggIdIconRect = SpriteRect(349, 607, 48, 48);
const FIntRect NewRect = SpriteRect(1154, 431, 53, 56);
const FIntRect RedPointRect = SpriteRect(2018, 994, 27, 29);
const FIntRect ItemTopRect = SpriteRect(974, 885, 120, 27);
const FIntRect LargeBlueButtonRect = SpriteRect(1490, 617, 274, 84);
const FIntRect MediumBlueButtonRect = SpriteRect(1098, 571, 202, 81);
const FIntRect MediumGoldButtonRect = SpriteRect(677, 600, 202, 81);
const FIntRect WorkerLockedRect = SpriteRect(1, 659, 179, 137);
const FIntRect WorkerBusyRect = SpriteRect(182, 1215, 179, 137);
const FIntRect WorkerIdleRect = SpriteRect(182, 1354, 179, 137);
const FIntRect BuildCityCardDarkRect = SpriteRect(1, 19, 159, 501);
const FIntRect BuildCityCardLightRect = SpriteRect(1, 522, 159, 501);
const FIntRect BuildCitySideDecorationRect = SpriteRect(162, 699, 158, 324);
const FIntRect BuildCitySelectedTabRect = SpriteRect(162, 601, 122, 96);
const FIntRect BuildCityNormalTabRect = SpriteRect(322, 939, 110, 84);
const FIntRect BuildCityDrawerRect = SpriteRect(129, 110, 100, 100);
const FIntRect StoneRect = SpriteRect(947, 431, 80, 80);
const FIntRect GoldRect = SpriteRect(865, 431, 80, 80);
const FIntRect MiniBlueButtonRect = SpriteRect(1211, 474, 138, 54);
const FIntRect MiniGreenButtonRect = SpriteRect(1351, 474, 138, 54);
const FIntRect PageSelectedRect = SpriteRect(1766, 539, 214, 45);
const FIntRect PageNormalRect = SpriteRect(205, 475, 215, 38);
const FIntRect SideTabSelectedRect = SpriteRect(205, 370, 145, 103);
const FIntRect SideTabDarkRect = SpriteRect(205, 266, 121, 102);
const FIntRect AlliancePageIconRects[3][2] = {
	{SpriteRect(1, 561, 60, 60), SpriteRect(1, 623, 60, 60)},
	{SpriteRect(1, 685, 60, 60), SpriteRect(1, 747, 60, 60)},
	{SpriteRect(63, 1057, 60, 60), SpriteRect(63, 1119, 60, 60)},
};
const FIntRect FoodRect = SpriteRect(865, 349, 80, 80);
const FIntRect WoodRect = SpriteRect(1085, 489, 80, 80);
const FIntRect GemRect = SpriteRect(783, 435, 80, 80);
const FIntRect CommanderRareFrameRect = SpriteRect(1, 53, 134, 152);
const FIntRect CommanderEpicFrameRect = SpriteRect(729, 1895, 134, 152);
const FIntRect CommanderLegendaryFrameRect = SpriteRect(593, 1895, 134, 152);
const FIntRect CommanderBackRect = SpriteRect(604, 611, 69, 69);
const FIntRect CommanderStarOnRect = SpriteRect(436, 222, 64, 64);
const FIntRect CommanderStarOffRect = SpriteRect(1929, 473, 64, 64);
struct FCommanderHeadSource
{
	int32 HeroId;
	FIntRect Rect;
};

const FCommanderHeadSource CommanderHeadSources[] = {
	{2007, SpriteRect(831, 1741, 134, 152)},
	{1006, SpriteRect(817, 1433, 134, 152)},
	{1013, SpriteRect(1, 1836, 148, 211)},
	{1014, SpriteRect(151, 1867, 161, 180)},
	{1000, SpriteRect(137, 943, 134, 152)},
	{1001, SpriteRect(1, 912, 134, 152)},
	{1002, SpriteRect(817, 1125, 134, 152)},
	{1003, SpriteRect(817, 1279, 134, 152)},
	{1005, SpriteRect(681, 1125, 134, 152)},
};
const FIntRect CommanderSkillRects[] = {
	SpriteRect(901, 1149, 223, 223),
	SpriteRect(1126, 1599, 223, 223),
	SpriteRect(901, 1374, 223, 223),
	SpriteRect(1801, 1824, 223, 223),
	SpriteRect(1576, 1824, 223, 223),
};

bool ResolveFallbackCommanderHeadRect(
	int32 HeroId,
	FIntRect& OutRect)
{
	for (const FCommanderHeadSource& Source : CommanderHeadSources)
	{
		if (Source.HeroId == HeroId)
		{
			OutRect = Source.Rect;
			return true;
		}
	}
	return false;
}

const FIntRect ItemQualityCommonRect = SpriteRect(137, 219, 134, 134);
const FIntRect ItemQualityBlueRect = SpriteRect(295, 377, 134, 134);
const FIntRect ItemQualityPurpleRect = SpriteRect(159, 377, 134, 134);
const FIntRect ItemQualityEpicRect = SpriteRect(1, 83, 134, 134);
const FIntRect ItemQualityLegendaryRect = SpriteRect(1, 219, 134, 134);
const FIntRect ItemSelectedRect = SpriteRect(1, 355, 156, 156);
const FIntRect ItemMinusRect = SpriteRect(1365, 422, 50, 50);
const FIntRect ItemPlusRect = SpriteRect(1417, 422, 50, 50);
const FIntRect ItemMaxRect = SpriteRect(1208, 654, 87, 51);

// UI_IF_SearchRes samples the original Unity map_icon/ui_common atlases.
// The type array remains in protocol order: Barbarian, Food, Wood, Stone, Gold.
const FIntRect ResourceSearchTypeIconRects[] = {
	SpriteRect(547, 1847, 180, 200),
	SpriteRect(183, 231, 180, 200),
	SpriteRect(183, 433, 180, 200),
	SpriteRect(183, 635, 180, 200),
	SpriteRect(183, 837, 180, 200),
};
const FIntRect ResourceSearchTypeFrameRect = SpriteRect(365, 1548, 137, 151);
const FIntRect ResourceSearchPopupRect = SpriteRect(341, 80, 58, 55);
const FIntRect ResourceSearchPopupArrowRect = SpriteRect(191, 270, 12, 15);
const FIntRect ResourceSearchSliderBackgroundRect = SpriteRect(401, 112, 20, 23);
const FIntRect ResourceSearchSliderFillRect = SpriteRect(384, 59, 12, 19);
const FIntRect ResourceSearchSliderThumbRect = SpriteRect(679, 474, 20, 40);
constexpr float HospitalSliderFillWidth = 336.0f;
const FIntRect ResourceSearchSubmitRect = SpriteRect(1766, 586, 274, 84);

const FIntRect AllianceJoinBannerRect = SpriteRect(1, 1735, 1016, 312);
const FIntRect AllianceStarRect = SpriteRect(148, 119, 64, 64);
const FIntRect AllianceFlagBackingRect = SpriteRect(1, 1362, 304, 371);
const FIntRect AllianceFlagClothRect = SpriteRect(187, 523, 184, 216);
const FIntRect AllianceFlagEmblemRect = SpriteRect(739, 1927, 120, 120);
// MakeAtlasBrush consumes the original Unity bottom-left sprite coordinates
// and performs the vertical UV conversion when it builds the Slate brush.
const FIntRect ChatCloseRect = SpriteRect(1, 3, 48, 124);
const FIntRect ChatSendRect = SpriteRect(51, 10, 56, 35);
const FIntRect ChatWorldIconRect = SpriteRect(241, 53, 74, 74);
const FIntRect ChatAllianceIconRect = SpriteRect(317, 53, 74, 74);
const FIntRect ChatSelectedContactRect = SpriteRect(393, 63, 64, 64);
const FIntRect ChatShrinkRect = SpriteRect(599, 57, 32, 32);
const FIntRect EmojiTextRect = SpriteRect(1, 127, 88, 88);
const FIntRect EmojiFaceRect = SpriteRect(1, 37, 88, 88);
const FIntRect EmojiPanelRect = SpriteRect(1, 217, 124, 70);
FIntRect AllianceOfficeIconRect(const FString& SpriteName)
{
	if (SpriteName == TEXT("icon_ali_achi1")) return SpriteRect(1, 942, 160, 160);
	if (SpriteName == TEXT("icon_ali_achi2")) return SpriteRect(830, 1071, 160, 160);
	if (SpriteName == TEXT("icon_ali_achi3")) return SpriteRect(823, 1233, 160, 160);
	if (SpriteName == TEXT("icon_ali_achi4")) return SpriteRect(823, 1395, 160, 160);
	return FIntRect(0, 0, 0, 0);
}

const FIntRect AllianceFeatureIconRects[] = {
	SpriteRect(751, 924, 145, 145),
	SpriteRect(604, 803, 145, 145),
	SpriteRect(457, 804, 145, 145),
	SpriteRect(310, 805, 145, 145),
	SpriteRect(163, 805, 145, 145),
	SpriteRect(604, 950, 145, 145),
	SpriteRect(457, 952, 145, 145),
	SpriteRect(310, 952, 145, 145),
};
const FIntRect VipLevelIconRects[] = {
	SpriteRect(1, 325, 138, 138),
	SpriteRect(1, 465, 138, 138),
	SpriteRect(1, 465, 138, 138),
	SpriteRect(281, 605, 138, 138),
	SpriteRect(281, 605, 138, 138),
	SpriteRect(281, 745, 138, 138),
	SpriteRect(141, 605, 138, 138),
	SpriteRect(141, 605, 138, 138),
	SpriteRect(141, 745, 138, 138),
	SpriteRect(281, 885, 138, 138),
	SpriteRect(141, 885, 138, 138),
	SpriteRect(141, 885, 138, 138),
	SpriteRect(1, 605, 138, 138),
	SpriteRect(1, 745, 138, 138),
	SpriteRect(1, 745, 138, 138),
	SpriteRect(1, 885, 138, 138),
	SpriteRect(1, 885, 138, 138),
	SpriteRect(1, 885, 138, 138),
};
constexpr int32 VipLevelCount = static_cast<int32>(UE_ARRAY_COUNT(VipLevelIconRects));
const FIntRect VipDailyTitleRect = SpriteRect(1, 853, 458, 84);
const FIntRect VipSpecialTitleRect = SpriteRect(1, 939, 458, 84);
const FIntRect VipCurrentPointRect = SpriteRect(947, 349, 80, 80);
const FIntRect VipProgressFillRect = SpriteRect(384, 59, 12, 19);
const FIntRect VipHelpRect = SpriteRect(743, 237, 37, 37);
const FIntRect VipAddRect = SpriteRect(1417, 422, 50, 50);
const FIntRect VipPageArrowRect = SpriteRect(1096, 654, 110, 276);
const FIntRect VipDailyPointChestRect = SpriteRect(1090, 177, 171, 136);
const FIntRect VipFreeChestRect = SpriteRect(940, 186, 148, 182);
const FIntRect VipSpecialChestRect = SpriteRect(972, 2, 146, 173);

struct FWorldMapPreviewMarker
{
	int32 Category;
	const TCHAR* Name;
	const TCHAR* Description;
	FVector2D DisplayCoordinate;
};

const FWorldMapPreviewMarker WorldMapPreviewMarkers[] = {
	{1, TEXT("远古遗迹"), TEXT("特殊标记"), FVector2D(1088.0f, 236.0f)},
	{2, TEXT("盟友集结点"), TEXT("友方标记"), FVector2D(1114.0f, 181.0f)},
	{3, TEXT("敌军营地"), TEXT("敌方标记"), FVector2D(1180.0f, 202.0f)},
	{4, TEXT("联盟要塞"), TEXT("联盟标记"), FVector2D(1036.0f, 278.0f)},
};
constexpr int32 WorldMapPreviewMarkerCount =
	static_cast<int32>(UE_ARRAY_COUNT(WorldMapPreviewMarkers));

FText FormatMapMarkerDescription(const FWorldMapPreviewMarker& Marker)
{
	return FText::Format(
		NSLOCTEXT("RokMapMarker", "MarkerDescription", "{0}  X:{1} Y:{2}"),
		FText::FromString(Marker.Description),
		FText::AsNumber(FMath::RoundToInt(Marker.DisplayCoordinate.X)),
		FText::AsNumber(FMath::RoundToInt(Marker.DisplayCoordinate.Y)));
}

bool DoesMarkerMatchPage(
	const FWorldMapPreviewMarker& Marker,
	int32 Page)
{
	return Page == 0 || Marker.Category == Page;
}

int32 FindFirstMarkerForPage(int32 Page)
{
	for (int32 Index = 0; Index < WorldMapPreviewMarkerCount; ++Index)
	{
		if (DoesMarkerMatchPage(WorldMapPreviewMarkers[Index], Page))
		{
			return Index;
		}
	}
	return 0;
}

const FMargin WindowBodyMargin(493.0f / 524.0f, 278.0f / 659.0f, 15.0f / 524.0f, 291.0f / 659.0f);
const FMargin WindowHeaderMargin(539.0f / 560.0f, 0.0f, 8.0f / 560.0f, 0.0f);
const FMargin ActivityFrameMargin(47.0f / 118.0f, 324.0f / 392.0f, 45.0f / 118.0f, 57.0f / 392.0f);
const FMargin CommonPanelMargin(16.0f / 63.0f, 13.0f / 55.0f, 15.0f / 63.0f, 13.0f / 55.0f);
const FMargin TaskRowMargin(23.0f / 74.0f, 13.0f / 67.0f, 21.0f / 74.0f, 22.0f / 67.0f);
const FMargin SideNormalMargin(18.0f / 41.0f, 0.0f, 15.0f / 41.0f, 0.0f);
const FMargin SideSelectedMargin(38.0f / 61.0f, 0.0f, 14.0f / 61.0f, 0.0f);
const FMargin MiniBlueMargin(70.0f / 138.0f, 0.0f, 64.0f / 138.0f, 0.0f);
const FMargin MiniGreenMargin(62.0f / 138.0f, 0.0f, 65.0f / 138.0f, 0.0f);
const FMargin ResourceSearchPopupMargin(
	25.0f / 58.0f,
	21.0f / 55.0f,
	18.0f / 58.0f,
	25.0f / 55.0f);
const FMargin ResourceSearchSliderBackgroundMargin(
	9.0f / 20.0f,
	6.0f / 23.0f,
	9.0f / 20.0f,
	7.0f / 23.0f);

constexpr float UnityReferenceWidth = 1440.0f;
constexpr float BuildCityListLeft = 144.0f;
constexpr float BuildCityCardStride = 332.0f;
constexpr float UnityReferenceHeight = 810.0f;

FString ResolveTaskDescription(const FString& Source, int32 Param1, int32 Param2, int32 Require)
{
	FString Result = Source;
	const FString P1 = FText::AsNumber(Param1).ToString();
	const FString P2 = FText::AsNumber(Param2).ToString();
	const FString P3 = FText::AsNumber(Require).ToString();
	Result.ReplaceInline(TEXT("{p1}"), *P1, ESearchCase::CaseSensitive);
	Result.ReplaceInline(TEXT("{p2}"), *P2, ESearchCase::CaseSensitive);
	Result.ReplaceInline(TEXT("{p3}"), *P3, ESearchCase::CaseSensitive);
	return Result;
}

FString ResolveEconomyItemDescription(const FRokUnityEconomyItem& Item)
{
	if (Item.DescriptionTemplateZhCn.IsEmpty())
	{
		return TEXT("Unity Item 配置未提供可解析说明。");
	}

	FString Result = Item.DescriptionTemplateZhCn;
	const FString Data1 =
		FText::AsNumber(Item.DescriptionData1).ToString();
	const FString Data2 =
		FText::AsNumber(Item.DescriptionData2).ToString();
	Result.ReplaceInline(
		TEXT("{0}"),
		*Data1,
		ESearchCase::CaseSensitive);
	Result.ReplaceInline(
		TEXT("{1}"),
		*Data2,
		ESearchCase::CaseSensitive);
	return Result;
}

bool ResolveEconomyItemIcon(
	const FRokUnityEconomyItem& Item,
	const TCHAR*& OutAtlasPath,
	FIntRect& OutRect,
	FIntPoint& OutAtlasSize)
{
	OutAtlasPath = nullptr;
	if (Item.IconAtlas == TEXT("item1"))
	{
		OutAtlasPath = Item1AtlasPath;
	}
	else if (Item.IconAtlas == TEXT("item2"))
	{
		OutAtlasPath = Item2AtlasPath;
	}
	else if (Item.IconAtlas == TEXT("item3"))
	{
		OutAtlasPath = Item3AtlasPath;
	}
	else if (Item.IconAtlas == TEXT("item4"))
	{
		OutAtlasPath = Item4AtlasPath;
	}
	else if (Item.IconAtlas == TEXT("item5"))
	{
		OutAtlasPath = Item5AtlasPath;
	}

	if (!OutAtlasPath
		|| Item.IconWidth <= 0
		|| Item.IconHeight <= 0
		|| Item.IconAtlasWidth <= 0
		|| Item.IconAtlasHeight <= 0)
	{
		return false;
	}
	OutRect = SpriteRect(
		Item.IconX,
		Item.IconY,
		Item.IconWidth,
		Item.IconHeight);
	OutAtlasSize = FIntPoint(
		Item.IconAtlasWidth,
		Item.IconAtlasHeight);
	return true;
}

FText FormatBuildDuration(int32 TotalSeconds);

FText ResolveEconomyItemTopLabel(const FRokUnityEconomyItem& Item)
{
	if (Item.SpeedDurationSeconds > 0)
	{
		return FormatBuildDuration(Item.SpeedDurationSeconds);
	}
	if (Item.TopData > 0)
	{
		return FText::AsNumber(Item.TopData);
	}
	return FText::GetEmpty();
}

FIntRect ResolveTaskIconRect(const FString& Icon)
{
	// Rects from ref/client-unity/Assets/BundleAssets/UI/UITextureStatic/
	// ui_res_task/task_icon.tpsheet (256x1024). task_icon_1003 stays the
	// explicit fallback for unknown sprite names.
	struct FTaskIconEntry
	{
		const TCHAR* SpriteName;
		FIntRect Rect;
	};
	static const FTaskIconEntry TaskIconEntries[] = {
		{TEXT("task_icon_1001"), SpriteRect(1, 5, 100, 100)},
		{TEXT("task_icon_1002"), SpriteRect(1, 107, 100, 100)},
		{TEXT("task_icon_1003"), SpriteRect(1, 209, 100, 100)},
		{TEXT("task_icon_1004"), SpriteRect(1, 311, 100, 100)},
		{TEXT("task_icon_1005"), SpriteRect(1, 413, 100, 100)},
		{TEXT("task_icon_1006"), SpriteRect(103, 515, 100, 100)},
		{TEXT("task_icon_1007"), SpriteRect(103, 617, 100, 100)},
		{TEXT("task_icon_1008"), SpriteRect(103, 719, 100, 100)},
		{TEXT("task_icon_1009"), SpriteRect(103, 821, 100, 100)},
		{TEXT("task_icon_1010"), SpriteRect(1, 515, 100, 100)},
		{TEXT("task_icon_1011"), SpriteRect(1, 617, 100, 100)},
		{TEXT("task_icon_1012"), SpriteRect(1, 719, 100, 100)},
		{TEXT("task_icon_1013"), SpriteRect(103, 923, 100, 100)},
		{TEXT("task_icon_1014"), SpriteRect(1, 821, 100, 100)},
		{TEXT("task_icon_1015"), SpriteRect(1, 923, 100, 100)},
	};
	for (const FTaskIconEntry& Entry : TaskIconEntries)
	{
		if (Icon.Contains(Entry.SpriteName))
		{
			return Entry.Rect;
		}
	}
	return SpriteRect(1, 209, 100, 100);
}

// Unity TaskProxy.GetTaskSideList (TaskProxy.cs:1163) for a fresh bootstrap
// account: first unfinished task per group1 in SortId order meeting
// showLevelReq / showBuildingReq, truncated to taskSideShowLimit=5
// (Config.csv taskSideShowLimit). Finished/received states need server task
// state and do not exist in the bootstrap fixture.
TArray<FRokUnityCatalogTask> ComputeBootstrapSideTaskCandidates(
	const URokUnityGameplayCatalogSubsystem* GameplayCatalog,
	const URokBuildingUpgradeSubsystem* BuildingUpgrade)
{
	// UE bootstrap assumption: a fresh account starts at level 1; no server
	// role state exists yet.
	constexpr int32 BootstrapRoleLevel = 1;
	constexpr int32 TaskSideShowLimit = 5;

	TArray<FRokUnityCatalogTask> Candidates;
	if (!GameplayCatalog)
	{
		return Candidates;
	}
	const TArray<int32> BuiltTypeIds = BuildingUpgrade
		? BuildingUpgrade->GetUnityInitialBuildingTypeIds()
		: TArray<int32>();
	TSet<int32> SeenGroups;
	for (const FRokUnityCatalogTask& Task : GameplayCatalog->GetSideTasksBySortOrder())
	{
		if (Candidates.Num() >= TaskSideShowLimit)
		{
			break;
		}
		if (SeenGroups.Contains(Task.Group1))
		{
			continue;
		}
		const bool bMeetsBuilding = Task.ShowBuildingReq == 0
			|| BuiltTypeIds.Contains(Task.ShowBuildingReq);
		if (BootstrapRoleLevel >= Task.ShowLevelReq && bMeetsBuilding)
		{
			SeenGroups.Add(Task.Group1);
			Candidates.Add(Task);
		}
	}
	return Candidates;
}

const TCHAR* TaskRowPageToken(FRokTaskRowState::EPage Page)
{
	return Page == FRokTaskRowState::EPage::MainTask ? TEXT("main")
		: Page == FRokTaskRowState::EPage::SideTask ? TEXT("side")
		: Page == FRokTaskRowState::EPage::ChapterTask ? TEXT("chapter") : TEXT("daily");
}

const TCHAR* TaskRowStateToken(FRokTaskRowState::EState State)
{
	switch (State)
	{
	case FRokTaskRowState::EState::Finished: return TEXT("finished");
	case FRokTaskRowState::EState::Received: return TEXT("received");
	case FRokTaskRowState::EState::Unfinished: return TEXT("unfinished");
	default: return TEXT("not_synced");
	}
}

const TCHAR* TaskRowIntentToken(FRokTaskRowState::EIntent Intent)
{
	switch (Intent)
	{
	case FRokTaskRowState::EIntent::Claim: return TEXT("claim");
	case FRokTaskRowState::EIntent::Go: return TEXT("go");
	default: return TEXT("inspect");
	}
}

FIntRect ResolveBagQualityRect(int32 QualityStyle)
{
	switch (FMath::Clamp(QualityStyle, 1, 5))
	{
	case 2: return ItemQualityBlueRect;
	case 3: return ItemQualityPurpleRect;
	case 4: return ItemQualityEpicRect;
	case 5: return ItemQualityLegendaryRect;
	default: return ItemQualityCommonRect;
	}
}

FText ResolveBuildingTypeName(int32 TypeId)
{
	switch (TypeId)
	{
	case 1: return NSLOCTEXT("RokBuildCity", "TownHall", "市政厅");
	case 2: return NSLOCTEXT("RokBuildCity", "Farm", "农场");
	case 3: return NSLOCTEXT("RokBuildCity", "LumberMill", "锯木厂");
	case 4: return NSLOCTEXT("RokBuildCity", "Quarry", "采矿场");
	case 5: return NSLOCTEXT("RokBuildCity", "GoldMine", "金矿");
	case 6: return NSLOCTEXT("RokBuildCity", "CityWall", "城墙");
	case 7: return NSLOCTEXT("RokBuildCity", "WorkerHut", "工人小屋");
	case 8: return NSLOCTEXT("RokBuildCity", "Watchtower", "哨塔");
	case 9: return NSLOCTEXT("RokBuildCity", "InfantryBarracks", "步兵营地");
	case 10: return NSLOCTEXT("RokBuildCity", "CavalryBarracks", "骑兵营地");
	case 11: return NSLOCTEXT("RokBuildCity", "ArcheryRange", "弓兵营地");
	case 12: return NSLOCTEXT("RokBuildCity", "SiegeWorkshop", "攻城器坊");
	case 13: return NSLOCTEXT("RokBuildCity", "Academy", "研究院");
	case 14: return NSLOCTEXT("RokBuildCity", "Hospital", "医院");
	case 15: return NSLOCTEXT("RokBuildCity", "Storehouse", "仓库");
	case 16: return NSLOCTEXT("RokBuildCity", "AllianceCenter", "联盟中心");
	case 17: return NSLOCTEXT("RokBuildCity", "Castle", "城堡");
	case 18: return NSLOCTEXT("RokBuildCity", "CommanderHall", "统帅厅");
	case 19: return NSLOCTEXT("RokBuildCity", "TradingPost", "交易所");
	case 20: return NSLOCTEXT("RokBuildCity", "Shop", "商店");
	case 21: return NSLOCTEXT("RokBuildCity", "Market", "市集");
	case 28: return NSLOCTEXT("RokBuildCity", "ScoutCamp", "斥候营地");
	case 29: return NSLOCTEXT("RokBuildCity", "NoticeBoard", "公告板");
	case 30: return NSLOCTEXT("RokBuildCity", "Monument", "奇观");
	case 31: return NSLOCTEXT("RokBuildCity", "Blacksmith", "铁匠铺");
	case 51:
	case 52:
	case 53:
		return NSLOCTEXT("RokBuildCity", "PalmTree", "棕榈树");
	case 54:
	case 55:
		return NSLOCTEXT("RokBuildCity", "MapleTree", "枫树");
	default:
		return FText::Format(
			NSLOCTEXT("RokBuildCity", "BuildingTypeFallback", "建筑 {0}"),
			FText::AsNumber(TypeId));
	}
}

FText ResolveBuildCityDescription(
	const FRokUnityCatalogBuildingType& BuildingType)
{
	const FString& SourceText =
		!BuildingType.DisplayShortDescriptionZhCn.IsEmpty()
			? BuildingType.DisplayShortDescriptionZhCn
			: BuildingType.DisplayDescriptionZhCn;
	if (!SourceText.IsEmpty())
	{
		return FText::FromString(SourceText);
	}
	return FText::Format(
		NSLOCTEXT(
			"RokBuildCity",
			"CatalogDescriptionFallback",
			"占地 {0}×{1}，一级战力 +{2}"),
		FText::AsNumber(BuildingType.Width),
		FText::AsNumber(BuildingType.Length),
		FText::AsNumber(BuildingType.Level1Power));
}

FText FormatBuildDuration(int32 TotalSeconds)
{
	if (TotalSeconds <= 0)
	{
		return NSLOCTEXT("RokBuildCity", "InstantBuild", "立即");
	}
	if (TotalSeconds < 60)
	{
		return FText::Format(
			NSLOCTEXT("RokBuildCity", "SecondsFormat", "{0}秒"),
			FText::AsNumber(TotalSeconds));
	}
	if (TotalSeconds < 3600)
	{
		return FText::Format(
			NSLOCTEXT("RokBuildCity", "MinutesFormat", "{0}分钟"),
			FText::AsNumber(FMath::CeilToInt(
				static_cast<float>(TotalSeconds) / 60.0f)));
	}
	return FText::Format(
		NSLOCTEXT("RokBuildCity", "HoursFormat", "{0}小时"),
		FText::AsNumber(FMath::CeilToInt(
			static_cast<float>(TotalSeconds) / 3600.0f)));
}

const FRokServerTrainingQueue* FindServerTrainingQueue(const FRokServerArmySnapshot& Snapshot, int32 BuildingIndex)
{
	if (BuildingIndex <= 0) return nullptr;
	return Snapshot.TrainingQueues.FindByPredicate([BuildingIndex](const FRokServerTrainingQueue& Queue)
	{
		return Queue.BuildingIndex == BuildingIndex;
	});
}

const FRokServerTrainingBuildingView* FindServerTrainingBuilding(
	const FRokServerArmySnapshot& Snapshot, int32 BuildingIndex, int32 ArmyType)
{
	return Snapshot.TrainingBuildings.FindByPredicate([BuildingIndex, ArmyType](const FRokServerTrainingBuildingView& Building)
	{
		return BuildingIndex > 0 && Building.BuildingIndex == BuildingIndex
			&& Building.BuildingType == ArmyType + 8;
	});
}

FText DescribeServerTrainingQueue(const FRokServerArmySnapshot& Snapshot, int32 BuildingIndex, int32 ArmyType)
{
	if (!Snapshot.bLoaded || !Snapshot.bTrainingKnown)
		return NSLOCTEXT("RokTrainArmy", "ServerTrainingWaiting", "等待服务器同步训练队列");
	if (!Snapshot.bBuildingsKnown || BuildingIndex <= 0
		|| !Snapshot.KnownTrainingBuildingIndices.Contains(BuildingIndex)
		|| !FindServerTrainingBuilding(Snapshot, BuildingIndex, ArmyType))
		return NSLOCTEXT("RokTrainArmy", "ServerTrainingBuildingUnknown", "训练建筑状态待同步");
	const FRokServerTrainingQueue* Queue = FindServerTrainingQueue(Snapshot, BuildingIndex);
	if (!Queue || Queue->State == ERokServerTrainingState::Idle)
		return NSLOCTEXT("RokTrainArmy", "ServerTrainingIdle", "训练队列空闲");
	if (Queue->State == ERokServerTrainingState::Unknown)
		return NSLOCTEXT("RokTrainArmy", "ServerTrainingUnknown", "训练队列状态待同步");
	if (Queue->State == ERokServerTrainingState::AwaitingReceive)
		return FText::Format(NSLOCTEXT("RokTrainArmy", "ServerTrainingAwaiting", "{0} ×{1} · 训练已完成，待领取"),
			Queue->TroopName.IsEmpty() ? NSLOCTEXT("RokTrainArmy", "ReadyTroops", "部队") : FText::FromString(Queue->TroopName),
			FText::AsNumber(Queue->Count));
	const FText Action = Queue->State == ERokServerTrainingState::Upgrading
		? NSLOCTEXT("RokTrainArmy", "ServerTrainingUpgrading", "晋升中")
		: NSLOCTEXT("RokTrainArmy", "ServerTrainingActive", "训练中");
	const FText Time = Queue->bTimeKnown
		? FText::Format(NSLOCTEXT("RokTrainArmy", "ServerTrainingRemaining", "剩余 {0}"),
			FormatBuildDuration(FMath::Max(0, FMath::CeilToInt(Queue->RemainingSeconds))))
		: NSLOCTEXT("RokTrainArmy", "ServerTrainingTimeUnknown", "时间待同步");
	return FText::Format(NSLOCTEXT("RokTrainArmy", "ServerTrainingQueueFormat", "{0} ×{1} · {2} · {3}"),
		FText::FromString(Queue->TroopName.IsEmpty() ? TEXT("部队") : Queue->TroopName),
		FText::AsNumber(Queue->Count), Action, Time);
}

FText FormatBuildCost(const FRokResourceCost& Cost)
{
	TArray<FString> Parts;
	if (Cost.Food > 0)
	{
		Parts.Add(FString::Printf(TEXT("食物 %d"), Cost.Food));
	}
	if (Cost.Wood > 0)
	{
		Parts.Add(FString::Printf(TEXT("木材 %d"), Cost.Wood));
	}
	if (Cost.Stone > 0)
	{
		Parts.Add(FString::Printf(TEXT("石料 %d"), Cost.Stone));
	}
	if (Cost.Gold > 0)
	{
		Parts.Add(FString::Printf(TEXT("金币 %d"), Cost.Gold));
	}
	return Parts.IsEmpty()
		? NSLOCTEXT("RokBuildCity", "NoBuildCost", "无资源消耗")
		: FText::FromString(FString::Join(Parts, TEXT("  ")));
}

FText ResolvePersonalityToggleLabel(FName ToggleName)
{
	if (ToggleName == TEXT("HideMenusWhileScrollingMap"))
	{
		return NSLOCTEXT(
			"RokGeneralSettings",
			"HideMenusWhileScrollingMap",
			"拖动地图时隐藏菜单");
	}
	if (ToggleName == TEXT("ConfirmDiamondSpending"))
	{
		return NSLOCTEXT(
			"RokGeneralSettings",
			"ConfirmDiamondSpending",
			"消耗宝石时确认");
	}
	if (ToggleName == TEXT("ConfirmRecruitPurchase"))
	{
		return NSLOCTEXT(
			"RokGeneralSettings",
			"ConfirmRecruitPurchase",
			"招募购买时确认");
	}
	if (ToggleName == TEXT("BlockEmotesAndQuickReplies"))
	{
		return NSLOCTEXT(
			"RokGeneralSettings",
			"BlockEmotesAndQuickReplies",
			"屏蔽表情与快捷回复");
	}
	if (ToggleName
		== TEXT("ConfirmAllianceShopPersonalPointSpending"))
	{
		return NSLOCTEXT(
			"RokGeneralSettings",
			"ConfirmAllianceShopPersonalPointSpending",
			"消耗个人积分时确认");
	}
	if (ToggleName
		== TEXT("ConfirmAllianceShopAlliancePointSpending"))
	{
		return NSLOCTEXT(
			"RokGeneralSettings",
			"ConfirmAllianceShopAlliancePointSpending",
			"消耗联盟积分时确认");
	}
	return FText::FromName(ToggleName);
}

FText ResolveTroopTypeLabel(int32 ArmsType)
{
	switch (ArmsType)
	{
	case 1:
		return NSLOCTEXT("RokArmyStatistics", "Infantry", "步兵");
	case 2:
		return NSLOCTEXT("RokArmyStatistics", "Cavalry", "骑兵");
	case 3:
		return NSLOCTEXT("RokArmyStatistics", "Archer", "弓兵");
	case 4:
		return NSLOCTEXT("RokArmyStatistics", "Siege", "攻城");
	default:
		return FText::Format(
			NSLOCTEXT(
				"RokArmyStatistics",
				"UnknownType",
				"兵种 {0}"),
			FText::AsNumber(ArmsType));
	}
}

bool ResolveTroopHeadRect(
	int32 ArmsType,
	int32 Tier,
	FIntRect& OutRect)
{
	static const FIntRect StandardTroopRects[4][5] = {
		{
			SpriteRect(265, 513, 126, 126),
			SpriteRect(265, 641, 126, 126),
			SpriteRect(273, 769, 126, 126),
			SpriteRect(129, 103, 126, 126),
			SpriteRect(129, 231, 126, 126),
		},
		{
			SpriteRect(401, 897, 126, 126),
			SpriteRect(385, 1, 126, 126),
			SpriteRect(385, 129, 126, 126),
			SpriteRect(385, 257, 126, 126),
			SpriteRect(257, 1, 126, 126),
		},
		{
			SpriteRect(393, 513, 126, 126),
			SpriteRect(785, 897, 126, 126),
			SpriteRect(529, 769, 126, 126),
			SpriteRect(657, 897, 126, 126),
			SpriteRect(393, 641, 126, 126),
		},
		{
			SpriteRect(1, 51, 126, 126),
			SpriteRect(1, 179, 126, 126),
			SpriteRect(1, 307, 126, 126),
			SpriteRect(137, 743, 126, 126),
			SpriteRect(1, 435, 126, 126),
		},
	};
	if (ArmsType < 1 || ArmsType > 4 || Tier < 1 || Tier > 5)
	{
		return false;
	}
	OutRect = StandardTroopRects[ArmsType - 1][Tier - 1];
	return true;
}

FRokResourceCost ScaleResourceCost(
	const FRokResourceCost& Cost,
	int32 Quantity)
{
	const int32 SafeQuantity = FMath::Max(Quantity, 0);
	FRokResourceCost Result;
	Result.Food = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(Cost.Food) * SafeQuantity,
		0,
		MAX_int32));
	Result.Wood = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(Cost.Wood) * SafeQuantity,
		0,
		MAX_int32));
	Result.Stone = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(Cost.Stone) * SafeQuantity,
		0,
		MAX_int32));
	Result.Gold = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(Cost.Gold) * SafeQuantity,
		0,
		MAX_int32));
	return Result;
}

FText ResolveSpeedUpQueueLabel(int32 QueueType)
{
	switch (QueueType)
	{
	case 20102:
		return NSLOCTEXT("RokSpeedUp", "BuildingQueue", "建筑升级");
	case 20103:
		return NSLOCTEXT("RokSpeedUp", "TrainingQueue", "部队训练");
	case 20104:
		return NSLOCTEXT("RokSpeedUp", "ResearchQueue", "科技研究");
	case 20105:
		return NSLOCTEXT("RokSpeedUp", "HealingQueue", "伤兵治疗");
	default:
		return NSLOCTEXT("RokSpeedUp", "GenericQueue", "通用队列");
	}
}

FText ResolveChargePageLabel(int32 PagingType)
{
	switch (PagingType)
	{
	case 101:
		return NSLOCTEXT("RokCharge", "FirstCharge", "首充大礼包");
	case 102:
		return NSLOCTEXT("RokCharge", "RiseRoad", "崛起之路");
	case 103:
		return NSLOCTEXT("RokCharge", "SuperGift", "超值礼包");
	case 104:
		return NSLOCTEXT("RokCharge", "DailySpecial", "每日特惠");
	case 105:
		return NSLOCTEXT("RokCharge", "CitySupply", "城市补给");
	case 106:
		return NSLOCTEXT("RokCharge", "GrowthFund", "成长基金");
	case 107:
		return NSLOCTEXT("RokCharge", "GemShop", "宝石商店");
	default:
		return FText::Format(
			NSLOCTEXT("RokCharge", "UnknownPage", "充值页 {0}"),
			FText::AsNumber(PagingType));
	}
}
}

void URokUnityIndexedButton::ConfigureAction(int32 InActionId)
{
	ActionId = InActionId;
	if (!bClickHandlerBound)
	{
		OnClicked.AddUniqueDynamic(this, &URokUnityIndexedButton::HandleButtonClicked);
		bClickHandlerBound = true;
	}
}

void URokUnityIndexedButton::HandleButtonClicked()
{
	OnIndexedClicked.Broadcast(ActionId);
}

void URokUnityFeatureScreenWidget::OpenActivityCenter()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::ActivityCenter;
	SelectedActivityIndex = 1;
	SelectedActivityCalendarId = 0;
	ActivityCalendarScrollOffset = 0.0f;
	ActivityPreviewSecondsRemaining = 91824.0;
	LastRenderedActivitySecond = INDEX_NONE;
	const int32 PreviewTab = ReadLocalUiPreviewOption(TEXT("RokActivityPreviewTab="), 0, 2);
	bFreezeActivityCountdownForCapture = PreviewTab != INDEX_NONE;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	if (PreviewTab != INDEX_NONE && PreviewTab != SelectedActivityIndex)
	{
		const FName Name(*FString::Printf(TEXT("ActivityMenuHit_%d"), PreviewTab));
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(Name)); Button && Button->GetIsEnabled())
		{
			Button->OnClicked.Broadcast();
			UE_LOG(LogTemp, Display, TEXT("Rok activity preview: clicked %s."), *Name.ToString());
		}
	}
}

void URokUnityFeatureScreenWidget::OpenMail(bool bUseReferenceCaptureState)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Mail;
	SelectedMailTab = 0;
	SelectedMailIndex = 0;
	SelectedBattleReport = 0;
	bReferenceMailCaptureState = bUseReferenceCaptureState;
	bReadOnlyMailRewardFixtureState = false;
	bLocalMailUnreadFixtureState = false;
	MailRewardFixturePayloadByIndex.Reset();
	MailRewardFixtureError.Reset();
	RenderedMailRewardRows.Reset();
	ActiveMailRewardTooltipIndex = INDEX_NONE;
	MailRewardTooltipAnchor = FVector2D::ZeroVector;
	LastMailBoundaryActionId = INDEX_NONE;
	if (bUseReferenceCaptureState)
	{
		FRokMailRewardPayload ReferencePayload;
		if (RokMailRewardDetails::LoadFixture(ERokMailRewardFixture::ReferenceSingle, ReferencePayload, MailRewardFixtureError))
		{
			MailRewardFixturePayloadByIndex.Add(0, MoveTemp(ReferencePayload));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Rok mail reference reward fixture: %s"), *MailRewardFixtureError);
		}
	}
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	if (bUseReferenceCaptureState)
	{
		PreviewMailReferenceSelection();
	}
}

void URokUnityFeatureScreenWidget::OpenMailReadOnlyRewardFixture(
	const TMap<int32, FRokMailRewardPayload>& PayloadByMailIndex,
	int32 InitialMailIndex)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Mail;
	SelectedMailTab = 0;
	SelectedMailIndex = FMath::Clamp(InitialMailIndex, 0, 5);
	bReferenceMailCaptureState = true;
	bReadOnlyMailRewardFixtureState = true;
	bLocalMailUnreadFixtureState = false;
	MailRewardFixturePayloadByIndex = PayloadByMailIndex;
	MailRewardFixtureError.Reset();
	RenderedMailRewardRows.Reset();
	ActiveMailRewardTooltipIndex = INDEX_NONE;
	MailRewardTooltipAnchor = FVector2D::ZeroVector;
	LastMailBoundaryActionId = INDEX_NONE;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	PreviewMailReferenceSelection();
}

void URokUnityFeatureScreenWidget::OpenMailLocalUnreadFixture()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Mail;
	MailLocalUnreadState.SeedOnce();
	bLocalMailUnreadFixtureState = true;
	bReferenceMailCaptureState = false;
	bReadOnlyMailRewardFixtureState = false;
	SelectedMailTab = 0;
	SelectedMailIndex = MailLocalUnreadState.SelectFirstAndMarkReadForTab(SelectedMailTab);
	MailRewardFixturePayloadByIndex.Reset();
	RenderedMailRewardRows.Reset();
	ActiveMailRewardTooltipIndex = INDEX_NONE;
	LastMailBoundaryActionId = INDEX_NONE;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

#if !UE_BUILD_SHIPPING
FString URokUnityFeatureScreenWidget::GetVisibleItemsInventoryFingerprintForAudit() const
{
	FString Fingerprint;
	for (const FItemsInventoryStackState& Stack : VisibleItemsStacks)
	{
		Fingerprint += FString::Printf(TEXT("%lld:%d:%lld;"),
			static_cast<long long>(Stack.ItemIndex), Stack.ItemId,
			static_cast<long long>(Stack.Overlay));
	}
	return Fingerprint;
}

FString URokUnityFeatureScreenWidget::GetMailRewardFixtureFingerprintForAudit() const
{
	TArray<int32> Indices;
	MailRewardFixturePayloadByIndex.GetKeys(Indices);
	Indices.Sort();
	FString Fingerprint;
	for (const int32 Index : Indices)
	{
		const FRokMailRewardPayload& Payload = MailRewardFixturePayloadByIndex.FindChecked(Index);
		Fingerprint += FString::Printf(TEXT("%d:%lld,%lld,%lld,%lld,%lld|"), Index,
			static_cast<long long>(Payload.Food), static_cast<long long>(Payload.Wood),
			static_cast<long long>(Payload.Stone), static_cast<long long>(Payload.Gold),
			static_cast<long long>(Payload.Denar));
		for (const FRokMailRewardAmount& Soldier : Payload.Soldiers)
		{
			Fingerprint += FString::Printf(TEXT("S%lld:%lld;"),
				static_cast<long long>(Soldier.Id), static_cast<long long>(Soldier.Quantity));
		}
		Fingerprint += TEXT("|");
		for (const FRokMailRewardAmount& Item : Payload.Items)
		{
			Fingerprint += FString::Printf(TEXT("I%lld:%lld;"),
				static_cast<long long>(Item.Id), static_cast<long long>(Item.Quantity));
		}
		Fingerprint += TEXT("#");
	}
	return Fingerprint;
}
#endif

bool URokUnityFeatureScreenWidget::TryResolveLocalTask200900GoTarget(
	const FRokTaskRowState& Row, const FRokUnityRuntimeTaskState& Task,
	const FRokUnityRuntimePlayerProfileState& Profile, bool bRuntimeLoaded,
	UWorld* World, AActor*& OutActor, FName& OutFailure)
{
	OutActor = nullptr;
	OutFailure = TEXT("unsupported_task");
	if (!bRuntimeLoaded || Profile.StateKind != TEXT("local_preview")
		|| Profile.bHasRoleId || !Profile.RoleId.IsEmpty()
		|| Profile.bHasCivilization || Profile.CivilizationId != 0 || !Profile.CivilizationName.IsEmpty()
		|| Row.TaskId != 200900 || Row.Page != FRokTaskRowState::EPage::MainTask
		|| Row.State != FRokTaskRowState::EState::Unfinished
		|| Row.Intent != FRokTaskRowState::EIntent::Go
		|| Row.SourceAuthority != FName(TEXT("runtime_bootstrap"))
		|| Row.Type != 8 || Row.Param1 != 1 || Row.Param2 != 2
		|| Task.TaskId != Row.TaskId || Task.Type != Row.Type
		|| Task.Param1 != Row.Param1 || Task.Param2 != Row.Param2
		|| Task.Target != 2 || Task.ProgressRule != TEXT("building_level")
		|| Task.bComplete) return false;
	OutFailure = TEXT("unsupported_world");
	if (!World || !World->IsGameWorld()
		|| !World->GetMapName().EndsWith(TEXT("L_RokUnityGameAppCity"))) return false;
	TArray<AActor*> HallActors;
	UGameplayStatics::GetAllActorsWithTag(World, TEXT("UnityInitId_5"), HallActors);
	OutFailure = TEXT("missing_target");
	if (HallActors.IsEmpty()) return false;
	OutFailure = TEXT("ambiguous_target");
	if (HallActors.Num() != 1) return false;
	AActor* Hall = HallActors[0];
	OutFailure = TEXT("identity_mismatch");
	if (!IsValid(Hall) || Hall->GetWorld() != World
		|| !Hall->ActorHasTag(TEXT("UnityBuildingType_1"))) return false;
	OutActor = Hall;
	OutFailure = NAME_None;
	return true;
}

void URokUnityFeatureScreenWidget::OpenTasks(bool bUseTestFixtureState, int32 PageIndex)
{
	ResetTaskCompletedRowAnimation();
	TaskCompletedAnimationFixtureCallbacks = 0;
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Tasks;
	SelectedTaskPage = PageIndex == INDEX_NONE ? 1 : FMath::Clamp(PageIndex, 0, 2);
	if (!bUseTestFixtureState && PageIndex == INDEX_NONE)
	{
		FString TaskPage;
		if (FParse::Value(FCommandLine::Get(), TEXT("RokTaskPage="), TaskPage))
		{
			SelectedTaskPage = TaskPage.Equals(TEXT("chapter"), ESearchCase::IgnoreCase) ? 0
				: TaskPage.Equals(TEXT("daily"), ESearchCase::IgnoreCase) ? 2 : 1;
		}
	}
	SelectedTaskChapterGroupId = 1;
	bSelectedTaskChapterMetadataKnown = false;
	RenderedTaskChapterTaskIds.Reset();
	RenderedTaskChapterTaskRewardPackageIds.Reset();
	RenderedTaskChapterAggregateRewardPackageIds.Reset();
	TaskChapterLoadError.Reset();
	TaskChapterRewardLoadError.Reset();
	RokTaskChapterConfigPreviewData::LoadAll(TaskChapterGroups, TaskChapterTasks, TaskChapterLoadError);
	RokTaskChapterRewardPreviewData::LoadAll(TaskChapterRewardGroups, TaskChapterRewardLoadError);
	TaskProjectionRefreshAccumulator = 0.0f;
	LastServerTaskSignature.Reset();
	bLastServerTaskAvailable = false;
	bLastServerTaskLoaded = false;
	bTaskMainLocalProjectionRendered = false;
	bTaskMainLocalConditionMet = false;
	TaskMainDisplayedCurrent = 0;
	VisibleTaskStates.Reset();
	bTaskTestFixtureState = bUseTestFixtureState;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);

	if (!bUseTestFixtureState && PageIndex == INDEX_NONE && SelectedTaskPage == 0)
	{
		const int32 GroupId = ReadLocalUiPreviewOption(TEXT("RokTaskChapterPreviewGroup="), 2, 7);
		if (GroupId != INDEX_NONE)
		{
			const FName ButtonName(*FString::Printf(TEXT("TaskChapterGroupButton_%d"), GroupId));
			if (auto* Entry = Cast<UButton>(RokUiFind::Find(this, ButtonName)); Entry && Entry->GetIsEnabled())
			{
				Entry->OnClicked.Broadcast();
				UE_LOG(LogTemp, Display, TEXT("Rok task chapter local preview: clicked %s."), *ButtonName.ToString());
			}
		}
	}
}

void URokUnityFeatureScreenWidget::OpenServerTasksPage(int32 PageIndex)
{
	OpenTasks(false, PageIndex);
}

void URokUnityFeatureScreenWidget::OpenCommander(bool bUseLocalRosterFixture)
{
	StopCommanderSceneMotion();
	CommanderRosterAnimationTime = 0.0f;
	bCommanderRosterFirstTick = true;
	CommanderAnimatedPortrait = nullptr;
	CommanderAnimatedPortraitHeroId = INDEX_NONE;
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Commander;
	SelectedCommanderId = 2007;
	bSelectedCommanderServerOwned = false;
	bCommanderLocalRosterFixture = bUseLocalRosterFixture;
#if !UE_BUILD_SHIPPING
	bCommanderLocalRosterFixture |= FParse::Param(FCommandLine::Get(), TEXT("RokCommanderSyntheticSlateAuditV3"))
		|| FParse::Param(FCommandLine::Get(), TEXT("RokCommanderSkillReadonlyDetailAudit"))
		|| FParse::Param(FCommandLine::Get(), TEXT("RokHeroTalentReadonlyDetailAudit"))
		|| (FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
			&& FParse::Param(FCommandLine::Get(), TEXT("RokCommanderPreviewFixture")));
#endif
	bCommanderBiographyOpen = false;
	bCommanderServerSelectionInitialized = FParse::Value(
		FCommandLine::Get(), TEXT("RokCommanderHero="), SelectedCommanderId);
	SelectedCommanderSkillIndex = 0;
	SelectedCommanderSkillDetailIndex = INDEX_NONE;
	SelectedCommanderTalentIndex = INDEX_NONE;
	bCommanderTalentOpen = false;
	CommanderTalentPage = 1;
	CommanderConfirmation.Reset();
	LastCommanderInventorySignature.Reset();
	bCommanderSortMenuOpen = false;
	VisibleCommanderIds.Reset();
	VisibleCommanderTalentIds.Reset();
	VisibleCommanderSkillIds.Reset();
	ServerPlayerAssetsRefreshAccumulator = 0.0f;
	LastServerHeroSignature.Reset();
	bLastServerHeroAvailable = false;
	bLastServerHeroKnown = false;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenItems(bool bUseTestFixtureState)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Items;
	SelectedItemsCategory = 0;
	SelectedItemsIndex = 0;
	SelectedItemsStackIndex = 0;
	SelectedItemsUseCount = 1;
	bItemsTestFixtureState = bUseTestFixtureState
		|| FParse::Param(FCommandLine::Get(), TEXT("RokItemsPreviewFixture"));
	VisibleItemsStacks.Reset();
	ServerPlayerAssetsRefreshAccumulator = 0.0f;
	LastServerInventorySignature.Reset();
	bLastServerInventoryAvailable = false;
	bLastServerInventoryKnown = false;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::SelectCommanderHero(int32 HeroId)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Commander) return;
	const int32 Index = VisibleCommanderIds.IndexOfByKey(HeroId);
	if (Index != INDEX_NONE) ExecuteIndexedAction(400 + Index);
}

void URokUnityFeatureScreenWidget::SelectItemsCategory(int32 Category)
{
	if (ActiveMode == ERokUnityFeatureScreenMode::Items && Category >= 0 && Category < 5)
		ExecuteIndexedAction(520 + Category);
}

void URokUnityFeatureScreenWidget::SelectItemsStack(int64 ItemIndex)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Items) return;
	const int32 Index = VisibleItemsStacks.IndexOfByPredicate([ItemIndex](const FItemsInventoryStackState& Stack)
	{
		return Stack.ItemIndex == ItemIndex;
	});
	if (Index != INDEX_NONE) ExecuteIndexedAction(5000 + Index);
}

void URokUnityFeatureScreenWidget::ResetAllianceCreateLocalDraft()
{
	AllianceCreateFlagDraft = RokAllianceFlagLocalPreview::FDraft();
	AllianceFlagEditDraft = AllianceCreateFlagDraft;
	bAllianceFlagEditorOpen = false;
	SelectedAllianceFlagType = 1;
	AllianceCreateNameDraft.Reset();
	AllianceCreateTagDraft.Reset();
	AllianceCreateNoticeDraft.Reset();
}

void URokUnityFeatureScreenWidget::CaptureAllianceCreateFormDraft()
{
	auto ReadInput = [this](const FName Name)
	{
		if (const UEditableTextBox* Input = Cast<UEditableTextBox>(GetWidgetFromName(Name)))
		{
			return Input->GetText().ToString();
		}
		return FString();
	};
	AllianceCreateNameDraft = ReadInput(TEXT("AllianceCreateNameInput"));
	AllianceCreateTagDraft = ReadInput(TEXT("AllianceCreateTagInput"));
	AllianceCreateNoticeDraft = ReadInput(TEXT("AllianceCreateNoticeInput"));
}

void URokUnityFeatureScreenWidget::OpenAlliance(bool bOpenJoinedPreview)
{
	// A direct reopen replaces any prior close, including one awaiting a back action.
	PendingAllianceRootCloseActionId = INDEX_NONE;
	bDispatchingAllianceRootCloseAction = false;
	AllianceAnimatedRoot = EAllianceAnimatedRoot::None;
	AnimatedMode = ERokUnityFeatureScreenMode::None;
	WindowAnimation.Reset();
	PendingActionId = INDEX_NONE;
	ResetAllianceCreateLocalDraft();
	ActiveMode = ERokUnityFeatureScreenMode::Alliance;
	bAllianceTestFixtureState = bOpenJoinedPreview
		|| FParse::Param(
			FCommandLine::Get(),
			TEXT("RokAlliancePreviewFixture"));
	AlliancePreviewState = bOpenJoinedPreview ? 2 : 0;
	SelectedAllianceListIndex = 0;
	AllianceFixtureSearchInput.Reset();
	AllianceFixtureSearchQuery.Reset();
	VisibleAllianceFixtureIndices.Reset();
	SelectedAlliancePage = 0;
	SelectedAllianceOfficeIndex = 0;
	bAllianceTerritoryReferenceOpen = false;
	ExpandedAllianceTerritoryGroup = INDEX_NONE;
	SelectedAllianceTerritoryType = 0;
	bAllianceRankingReferenceOpen = false;
	SelectedAllianceRankingCategoryId = INDEX_NONE;
	bAllianceShopReferenceOpen = false;
	SelectedAllianceShopGroup = 1;
	SelectedAllianceShopItemType = 0;
	bAllianceGiftReferenceOpen = false;
	SelectedAllianceGiftConfiguredLevel = 1;
	bAllianceGiftSpecialDefinitions = false;
	bAllianceOfficeReferenceOpen = false;
	bAllianceTechnologyReferenceOpen = false;
	SelectedAllianceTechnologyCategory = 1;
	SelectedAllianceTechnologyStudyType = 0;
	SelectedAllianceTechnologyLevelKey = 1;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenVip(bool bUseTestFixtureState)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Vip;
	bVipTestFixtureState = bUseTestFixtureState
		|| FParse::Param(
			FCommandLine::Get(),
			TEXT("RokVipPreviewFixture"));
	SelectedVipPreviewLevel = 0;
	if (const int32 AccountLevel = ReadVipPresentation().CurrentLevel; AccountLevel != INDEX_NONE)
	{
		SelectedVipPreviewLevel = AccountLevel;
	}
	FParse::Value(
		FCommandLine::Get(),
		TEXT("RokVipLevel="),
		SelectedVipPreviewLevel);
	SelectedVipPreviewLevel = FMath::Clamp(SelectedVipPreviewLevel, 0, VipLevelCount - 1);
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenChat(bool bUseTestFixtureState)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Chat;
	SelectedChatChannel = 0;
	ChatMessageScrollOffset = 0.0f;
	bChatTestFixtureState = bUseTestFixtureState
		|| FParse::Param(FCommandLine::Get(), TEXT("RokChatPreviewFixture"));
	bChatEmojiPanelOpen = FParse::Param(
		FCommandLine::Get(),
		TEXT("RokChatEmojiOpen"));
	ChatDraft.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void URokUnityFeatureScreenWidget::OpenWorldPositionSearch()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::WorldPositionSearch;
	FIntPoint Minimum = FIntPoint::ZeroValue;
	FIntPoint Maximum = FIntPoint::ZeroValue;
	bool bLocalFallback = false;
	const bool bHasBounds = ResolveWorldPositionDisplayBounds(GetWorld(),
		Minimum, Maximum, bLocalFallback);
	if (APlayerController* Controller = GetOwningPlayer())
	{
		if (const ARokWorldMapCameraPawn* CameraPawn =
				Cast<ARokWorldMapCameraPawn>(Controller->GetPawn()))
		{
			const FVector2D DisplayPosition =
				CameraPawn->GetViewCenterDisplay();
			if (bHasBounds)
			{
				SearchCoordinateX = FMath::Clamp(FMath::RoundToInt(DisplayPosition.X),
					Minimum.X, Maximum.X);
				SearchCoordinateY = FMath::Clamp(FMath::RoundToInt(DisplayPosition.Y),
					Minimum.Y, Maximum.Y);
			}
		}
	}
	if (bHasBounds)
	{
		SearchCoordinateX = FMath::Clamp(SearchCoordinateX, Minimum.X, Maximum.X);
		SearchCoordinateY = FMath::Clamp(SearchCoordinateY, Minimum.Y, Maximum.Y);
	}
	UE_LOG(LogTemp, Log, TEXT("Rok coordinate search bounds: valid=%d source=%s min=(%d,%d) max=(%d,%d)."),
		bHasBounds, bLocalFallback ? TEXT("local_projection_fallback") : TEXT("world_config"),
		bHasBounds ? Minimum.X : 0, bHasBounds ? Minimum.Y : 0,
		bHasBounds ? Maximum.X : 0, bHasBounds ? Maximum.Y : 0);
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenMapMarkers()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::MapMarkers;
	SelectedMapMarkerPage = 0;
	SelectedMapMarkerIndex = 0;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenResourceSearch(int32 BarbarianLevel)
{
	const int32 GuideLevel = ReadLocalUiPreviewOption(TEXT("RokResourceSearchGuideLevel="), 1, 25);
	bFreezeResourceSearchGuideForCapture = GuideLevel != INDEX_NONE;
	if (BarbarianLevel <= 0 && bFreezeResourceSearchGuideForCapture)
	{
		BarbarianLevel = GuideLevel;
	}
	++ResourceSearchRequestSerial;
	bResourceSearchPending = false;
	bResourceSearchTaskGuide = BarbarianLevel > 0;
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::ResourceSearch;
	SelectedResourceSearchType = 0;
	ResourceSearchBarbarianLevel = FMath::Clamp(BarbarianLevel > 0 ? BarbarianLevel : ResourceSearchBarbarianLevel, 1, 25);
	ResourceSearchResourceLevel = FMath::Clamp(ResourceSearchResourceLevel, 1, 6);
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	const int32 Type = ReadLocalUiPreviewOption(TEXT("RokResourceSearchTypePreview="), 1, 4);
	if (Type != INDEX_NONE)
	{
		const FName Name(*FString::Printf(TEXT("ResourceSearchType_%d"), Type));
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(Name)); Button && Button->GetIsEnabled())
		{
			Button->OnClicked.Broadcast();
			UE_LOG(LogTemp, Display, TEXT("Rok resource search preview: clicked %s."), *Name.ToString());
		}
	}
}

void URokUnityFeatureScreenWidget::OpenExpedition()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Expedition;
	ExpeditionRows.Reset();
	ExpeditionLoadError.Reset();
	RokExpeditionStagePreviewData::LoadAll(ExpeditionRows, ExpeditionLoadError);
	RefreshExpeditionAccount();
	SelectedExpeditionStageId = ExpeditionRows.IsEmpty() ? 0 : ExpeditionRows[0].Id;
	for (const auto& Row : ExpeditionRows)
	{
		if (!CanChallengeExpedition(Row)) break;
		SelectedExpeditionStageId = Row.Id;
	}
	ExpeditionMapOffset = ExpeditionRows.IsValidIndex(SelectedExpeditionStageId - 1)
		? FMath::Max(0.0f, ExpeditionRows[SelectedExpeditionStageId - 1].BattleCoordinate.Y - 312.5f) : 0.0f;
	ExpeditionTeams.Reset();
	ExpeditionDraft = {};
	ExpeditionDraftSlot = INDEX_NONE;
	bExpeditionChoosingHero = false;
	bExpeditionPreview = false;
	bExpeditionTaskConfigVisible = false;
	ExpeditionLastBoundaryActionId = INDEX_NONE;
	ExpeditionAnimationPhase = NAME_None;
	ExpeditionAnimationElapsed = 0.0f;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenGemShop()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::GemShop;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenCharge(int32 InitialPageType)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Charge;
	SelectedChargePageType = FMath::Clamp(InitialPageType, 101, 107);
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenTaskCityTarget(int32 BuildingType, bool bOpenBuildCity, bool bUpgradeBuilding)
{
	if (bOpenBuildCity)
	{
		OpenBuildCity(BuildingType);
		return;
	}
	// Unity Research tasks locate the academy and guide the player to its study action.
	if (bUpgradeBuilding || BuildingType == 13)
	{
		const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
		if (City && City->HasAppliedServerCity() && !City->GetAppliedBuildings().ContainsByPredicate(
			[BuildingType](const FRokServerBuilding& Building) { return Building.Type == BuildingType; }))
		{
			OpenBuildCity(BuildingType);
			return;
		}
		auto* Controller = Cast<ARokPlayerController>(GetOwningPlayer());
		if (Controller && Controller->FocusTaskCityBuilding(BuildingType))
		{
			if (IsScreenOpen()) CloseScreen();
		}
		else ShowBoundaryFeedback(NSLOCTEXT("RokFeature", "TaskCityBuildingMissing", "找不到可用的任务建筑。"));
		return;
	}
	if (BuildingType == 14)
	{
		// Unity TaskScriptGlobalMediator.Healandcollect opens the hospital ring menu with the cure action.
		auto* Controller = Cast<ARokPlayerController>(GetOwningPlayer());
		if (Controller && Controller->FocusTaskHospital())
		{
			if (IsScreenOpen()) CloseScreen();
		}
		else
		{
			const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
			if (City && City->HasAppliedServerCity() && !City->GetAppliedBuildings().ContainsByPredicate(
				[](const FRokServerBuilding& Building) { return Building.Type == 14 && Building.Level > 0; }))
			{
				OpenBuildCity();
			}
			else ShowBoundaryFeedback(NSLOCTEXT("RokFeature", "TaskHospitalMissing", "找不到可用的医院。"));
		}
		return;
	}
	if (BuildingType < 9 || BuildingType > 12) return;
	const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	if (City)
	{
		for (const auto& Building : City->GetAppliedBuildings())
		{
			if (Building.Type != BuildingType || Building.Level <= 0) continue;
			if (AActor* Actor = City->FindBuildingBarAnchor(Building.Index))
			{
				OpenTrainArmy(Actor, BuildingType - 8);
				return;
			}
		}
	}
	ShowBoundaryFeedback(NSLOCTEXT("RokFeature", "TaskTrainingBuildingMissing", "找不到可用的训练建筑。"));
}

void URokUnityFeatureScreenWidget::OpenBuildCity(int32 GuideBuildingType)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::BuildCity;
	BuildCityGuideTypeId = GuideBuildingType;
	SelectedBuildCityCategory = 0;
	SelectedBuildCityTypeId = 0;
	VisibleBuildCityTypeIds.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	if (GuideBuildingType > 0 && !VisibleBuildCityTypeIds.Contains(GuideBuildingType))
	{
		ShowBoundaryFeedback(NSLOCTEXT("RokBuildCity", "GuideTargetMissing", "建造列表中找不到该建筑。"));
	}
}

void URokUnityFeatureScreenWidget::OpenWorkerQueue(AActor* BuildingActor, int32 RequiredSeconds)
{
	const bool bReturningFromSpeedUp = ActiveMode == ERokUnityFeatureScreenMode::SpeedUp && bSpeedUpFromWorkerQueue;
	const int32 Preview = ReadLocalUiPreviewOption(TEXT("RokWorkerQueueFixture="), 1, 3);
	WorkerRequiredSeconds = RequiredSeconds == 0 && Preview == 3 ? 8 : RequiredSeconds;
	ServerSpeedUpQueueIndex = 0;
	bSpeedUpFromWorkerQueue = false;
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::WorkerQueue;
	FeatureContextActor = BuildingActor;
	WorkerQueueRefreshAccumulator = 0.0f;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	if (!bReturningFromSpeedUp && GetBuildingSpeedUpPreview() != INDEX_NONE)
	{
		if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("WorkerQueuePrimaryAction"))); Button && Button->GetIsEnabled())
		{
			Button->OnClicked.Broadcast();
			UE_LOG(LogTemp, Display, TEXT("Rok building speed-up preview: clicked the primary worker."));
		}
	}
}

void URokUnityFeatureScreenWidget::OpenSettingsHub()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::SettingsHub;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenGeneralSettings()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::GeneralSettings;
	VisiblePersonalityToggleNames.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenNotificationSettings()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::NotificationSettings;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenHelp()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Help;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::BuildHelp()
{
	HelpWidget = WidgetTree->ConstructWidget<URokHelpWidget>(URokHelpWidget::ResolvePresentationClass(), TEXT("SettingsHelpWidget"));
	HelpWidget->InitializeHelp();
	HelpWidget->OnCloseRequested.AddUObject(this, &URokUnityFeatureScreenWidget::RequestCloseScreen);
	PlaceOnCanvas(ModeCanvas, HelpWidget, FVector2D::ZeroVector, FVector2D(1440, 810), 2);
}

void URokUnityFeatureScreenWidget::OpenLanguageSettings()
{
	LanguageDraftId = RokLanguageSettingsLayout::AppliedLanguageId;
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::LanguageSettings;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenAccountSettings()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::AccountSettings;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenArmyStatistics()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::ArmyStatistics;
	SelectedArmyStatisticsPage = 0;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	const int32 Tab = ReadLocalUiPreviewOption(TEXT("RokArmyStatisticsTab="), 1, 2);
	if (Tab != INDEX_NONE)
	{
		const FName Name(*FString::Printf(TEXT("ArmyStatisticsPage_%d"), Tab));
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(Name)); Button && Button->GetIsEnabled())
		{
			Button->OnClicked.Broadcast();
			UE_LOG(LogTemp, Display, TEXT("Rok army statistics preview: clicked %s."), *Name.ToString());
		}
	}
}

void URokUnityFeatureScreenWidget::OpenArmyStatisticsPage(int32 PageIndex)
{
	OpenArmyStatistics();
	SelectedArmyStatisticsPage = FMath::Clamp(PageIndex, 0, 2);
	if (SelectedArmyStatisticsPage != 0) RebuildModeContent();
}

void URokUnityFeatureScreenWidget::OpenStore()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Store;
	SelectedStoreCategory = 0;
	SelectedStorePage = 0;
	SelectedStoreItemIndex = 0;
	SelectedStoreQuantity = 1;
	VisibleStoreItemIds.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenTrainArmy(
	AActor* BuildingActor,
	int32 ArmyType)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::TrainArmy;
	FeatureContextActor = BuildingActor;
	SelectedServerTrainBuildingIndex = 0;
	SelectedTrainArmyType = FMath::Clamp(ArmyType, 1, 4);
	if (BuildingActor)
	{
		if (UWorld* World = GetWorld())
		{
			if (URokBuildingUpgradeSubsystem* UpgradeSubsystem =
					World->GetSubsystem<URokBuildingUpgradeSubsystem>())
			{
				const int32 UnityTypeId = UpgradeSubsystem
					->GetBuildingStateForActor(BuildingActor)
					.UnityBuildingTypeId;
				if (const auto* City = World->GetSubsystem<URokServerCitySubsystem>())
				{
					if (const auto* Building = City->FindBuildingForActor(BuildingActor))
						SelectedServerTrainBuildingIndex = Building->Index;
				}
				if (UnityTypeId >= 9 && UnityTypeId <= 12)
				{
					SelectedTrainArmyType = UnityTypeId - 8;
				}
			}
		}
	}
	const int32 ReplayIndex = ReadLocalUiPreviewOption(TEXT("RokTrainArmyPreviewServerBuilding="), 1, MAX_int32);
	if (ReplayIndex != INDEX_NONE)
	{
		const auto* Instance = GetGameInstance();
		const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
		const auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
		if (Net && Net->IsSnapshotReplay() && Army)
		{
			const FRokServerArmySnapshot Snapshot = Army->GetSnapshot();
			const auto* Building = Snapshot.TrainingBuildings.FindByPredicate([ReplayIndex](const auto& Candidate)
			{
				return Candidate.BuildingIndex == ReplayIndex;
			});
			if (Building)
			{
				SelectedServerTrainBuildingIndex = Building->BuildingIndex;
				SelectedTrainArmyType = Building->BuildingType - 8;
				UE_LOG(LogTemp, Display, TEXT("Rok training preview bound snapshot building index=%d army_type=%d."),
					SelectedServerTrainBuildingIndex, SelectedTrainArmyType);
			}
		}
	}
	SelectedTrainTier = 1;
	SelectedTrainCount = SelectedServerTrainBuildingIndex > 0 ? MAX_int32 : 1;
	bTrainArmyDetailVisible = false;
	bTrainArmyDetailFirstTick = false;
	TrainArmyDetailAnimationTime = 0.25f;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	const int32 PreviewTier = GetTrainArmyDetailPreviewTier();
	if (PreviewTier != INDEX_NONE)
	{
		ClickTrainArmyPreviewButton(PreviewTier == SelectedTrainTier ? FName(TEXT("TrainArmyDetailButton"))
			: FName(*FString::Printf(TEXT("TrainArmyTier_%d"), PreviewTier - 1)));
	}
	else if (const int32 BrowseTier = ReadLocalUiPreviewOption(TEXT("RokTrainArmyPreviewBrowse="), 2, 5); BrowseTier != INDEX_NONE)
	{
		ClickTrainArmyPreviewButton(*FString::Printf(TEXT("TrainArmyTier_%d"), BrowseTier - 1));
	}
	else if (ReadLocalUiPreviewOption(TEXT("RokTrainArmyPreviewConfirm="), 1, 1) == 1)
	{
		ClickTrainArmyPreviewButton(TEXT("TrainArmyInstant"));
	}
}

void URokUnityFeatureScreenWidget::ClickTrainArmyPreviewButton(FName Name)
{
	if (auto* Button = Cast<UButton>(GetWidgetFromName(Name)); Button && Button->GetIsEnabled())
	{
		Button->OnClicked.Broadcast();
		UE_LOG(LogTemp, Display, TEXT("Rok train army detail preview: clicked %s."), *Name.ToString());
	}
}

void URokUnityFeatureScreenWidget::OpenTrainArmyPreview(int32 ArmyType)
{
	OpenTrainArmy(nullptr, ArmyType);
}

void URokUnityFeatureScreenWidget::OpenTrainArmyServerBuilding(int32 BuildingIndex, int32 ArmyType)
{
	OpenTrainArmy(nullptr, ArmyType);
	SelectedServerTrainBuildingIndex = FMath::Max(0, BuildingIndex);
	SelectedTrainCount = SelectedServerTrainBuildingIndex > 0 ? MAX_int32 : 1;
	RebuildModeContent();
}

void URokUnityFeatureScreenWidget::OpenSpeedUp(
	AActor* QueueActor,
	int32 QueueType)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::SpeedUp;
	ServerSpeedUpQueueIndex = 0;
	bSpeedUpFromWorkerQueue = false;
	FeatureContextActor = QueueActor;
	SpeedUpQueueType = QueueType > 0 ? QueueType : 20102;
	FRokBuildingState BoundState;
	const FRokBuildingState* BoundStatePtr = nullptr;
	if (QueueActor && GetWorld())
	{
		if (auto* Buildings = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>())
		{
			BoundState = Buildings->GetBuildingStateForActor(QueueActor);
			BoundStatePtr = &BoundState;
		}
	}
	SpeedUpQueueLifecycle.Bind(SpeedUpQueueType, BoundStatePtr);
	SelectedSpeedUpPage = 0;
	SelectedSpeedUpItemIndex = 0;
	SpeedUpRefreshAccumulator = 0.0f;
	VisibleSpeedUpItemIds.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	if (ReadLocalUiPreviewOption(TEXT("RokSpeedUpPreviewPage="), 1, 1) == 1)
	{
		if (auto* Next = Cast<UButton>(GetWidgetFromName(TEXT("SpeedUpNextPage"))); Next && Next->GetIsEnabled())
		{
			Next->OnClicked.Broadcast();
		}
	}
}

void URokUnityFeatureScreenWidget::OpenHospital(AActor* BuildingActor)
{
	bHospitalExplanationVisible = false;
	HospitalState = {};
	HospitalCounts.Reset();
	HospitalRefreshAccumulator = 0;
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Hospital;
	FeatureContextActor = BuildingActor;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	if (GetHospitalPreviewState() == EHospitalLocalPreview::Confirm)
	{
		if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("HospitalHealNowButton"))); Button && Button->GetIsEnabled())
		{
			Button->OnClicked.Broadcast();
			UE_LOG(LogTemp, Display, TEXT("Rok hospital preview: clicked HospitalHealNowButton."));
		}
	}
}

void URokUnityFeatureScreenWidget::OpenResearch(AActor* AcademyActor)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Research;
	FeatureContextActor = AcademyActor;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenCityManager()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::CityManager;
	FeatureContextActor.Reset();
	SelectedCityManagerPage = 0;
	SelectedCityManagerGroupId = 0;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
	const int32 PreviewGroup = ReadLocalUiPreviewOption(TEXT("RokCityManagerPreviewGroup="), 201, 605);
	if (PreviewGroup != INDEX_NONE)
	{
		const FName Name(*FString::Printf(TEXT("CityManagerGroupButton_%d"), PreviewGroup));
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(Name)); Button && Button->GetIsEnabled())
		{
			Button->OnClicked.Broadcast();
			UE_LOG(LogTemp, Display, TEXT("Rok city manager preview: clicked %s."), *Name.ToString());
		}
	}
}

void URokUnityFeatureScreenWidget::OpenArmyShow()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::ArmyShow;
	// Source ShowArmys skips child Show on the first open.
	ArmyShowPanelAnimationTime = RokArmyShowPanelAnimation::Duration;
	FeatureContextActor.Reset();
	bArmyShowRulesVisible = false;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::CloseScreen()
{
	++ResourceSearchRequestSerial;
	bResourceSearchPending = false;
	ResetAllianceCreateLocalDraft();
	bAllianceRankingReferenceOpen = false;
	SelectedAllianceRankingCategoryId = INDEX_NONE;
	ResetTaskCompletedRowAnimation();
	if (ActiveMode == ERokUnityFeatureScreenMode::Hospital)
	{
		bHospitalExplanationVisible = false;
		HospitalExplanationWidget = nullptr;
		HospitalMainContent = nullptr;
		HospitalConfirmation = nullptr;
		HospitalQuantities.Reset();
	}
	ResearchScreenWidget = nullptr;
	if (HelpWidget) HelpWidget->ResetHelp();
	HelpWidget = nullptr;
	MailAttachmentPulse.Reset();
	for (auto& Pair : MailAttachmentMeshes)
	{
		if (Pair.Value) { Pair.Value->ResetModel(); Pair.Value->RemoveFromParent(); }
	}
	MailAttachmentMeshes.Reset();
	MailRewardFixturePayloadByIndex.Reset();
	RenderedMailRewardRows.Reset();
	MailRewardTooltipRoot = nullptr;
	VipDailyPointHelpTipRoot = nullptr;
	bVipDailyPointHelpVisible = false;
	ActiveMailRewardTooltipIndex = INDEX_NONE;
	MailRewardTooltipAnchor = FVector2D::ZeroVector;
	bReferenceMailCaptureState = false;
	bReadOnlyMailRewardFixtureState = false;
	bLocalMailUnreadFixtureState = false;
	SpeedUpQueueLifecycle.Reset();
	if (TrainArmyAnimatedModel) TrainArmyAnimatedModel->ResetModel();
	TrainArmyAnimatedModel = nullptr;
	TrainArmyTipLayer = nullptr;
	TrainArmyAnimatedChildren.Reset();
	bTrainArmyDetailVisible = false;
	bTrainArmyDetailFirstTick = false;
	TrainArmyDetailAnimationTime = 0.25f;
#if !UE_BUILD_SHIPPING
	bFreezeBuildCityAnimationForCapture = false;
#endif
	BuildCityAnimatedRoot = nullptr;
	BuildCityAnimatedCards.Reset();
	BuildCityCardSourceIndices.Reset();
	ResourceSearchAnimation.Reset();
	ResourceSearchDockRoot = nullptr;
	ResourceSearchPopupRoot = nullptr;
	StopCommanderSceneMotion();
	CommanderAnimatedPortrait = nullptr;
	CommanderAnimatedPortraitHeroId = INDEX_NONE;
	WindowAnimation.Reset();
	AllianceAnimatedRoot = EAllianceAnimatedRoot::None;
	PendingAllianceRootCloseActionId = INDEX_NONE;
	bDispatchingAllianceRootCloseAction = false;
	bResearchMainMaskClosing = false;
	ChatDrawerAnimation.Reset();
	ChatContactAnimation.Reset();
	ChatContentCanvas = nullptr;
	AnimatedMode = ERokUnityFeatureScreenMode::None;
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::None;
	ActiveModePage = nullptr;
	if (ModeCanvas)
	{
		URokUiAnimatedButton::ResetAnimationsInHierarchy(ModeCanvas);
		ModeCanvas->ClearChildren();
	}
	FeedbackText = nullptr;
	WorldPositionMiddleTipRoot = nullptr;
	WorldPositionMiddleTipBackground = nullptr;
	WorldPositionMiddleTipText = nullptr;
	WorldPositionMiddleTipElapsedSeconds = -1.0f;
	ActivityCountdownText = nullptr;
	ServerSpeedUpConfirmation = nullptr;
	ServerSpeedUpConfirmIndex = INDEX_NONE;
	SpeedUpQueueDescriptionText = nullptr;
	SpeedUpQueueProgress = nullptr;
	SpeedUpQueueProgressText = nullptr;
	WorkerQueuePrimaryTitleText = nullptr;
	WorkerQueueRuntimeStateText = nullptr;
	WorkerQueuePrimaryProgress = nullptr;
	WorkerQueuePrimaryProgressText = nullptr;
	WorkerQueueSecondaryTitleText = nullptr;
	WorkerQueueSecondaryNoteText = nullptr;
	WorkerQueueSecondaryProgress = nullptr;
	WorkerQueueSecondaryProgressText = nullptr;
	ChatInputTextBox = nullptr;
	WorldPositionXInput = nullptr;
	WorldPositionYInput = nullptr;
	ResourceSearchMonsterTip = nullptr;
	ResourceSearchGuide = nullptr;
	ResourceSearchSlider = nullptr;
	ResourceSearchSliderFill = nullptr;
	ResourceSearchLevelValueText = nullptr;
	FeatureContextActor.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
}

void URokUnityFeatureScreenWidget::RequestCloseScreen()
{
	if (ActiveMode == ERokUnityFeatureScreenMode::SpeedUp && bSpeedUpFromWorkerQueue)
	{
		OpenWorkerQueue(nullptr, WorkerRequiredSeconds);
		return;
	}
	if (!IsScreenOpen())
	{
		return;
	}
	// ResearchMainMediator.CloseRes calls CloseUI(..., true, true).
	if (ActiveMode == ERokUnityFeatureScreenMode::Research)
	{
		CloseScreen();
		return;
	}
	// These alliance reference pages have no verified root Animator.
	if (ActiveMode == ERokUnityFeatureScreenMode::Alliance
		&& AllianceAnimatedRoot == EAllianceAnimatedRoot::None)
	{
		CloseScreen();
		return;
	}
	if (WindowAnimation.IsClosing() || ChatDrawerAnimation.IsClosing() || ResourceSearchAnimation.IsClosing())
	{
		return;
	}
	if (HelpWidget) HelpWidget->SuspendInput();
	PendingActionId = INDEX_NONE;
	if (ActiveMode == ERokUnityFeatureScreenMode::ResourceSearch)
	{
		ResourceSearchAnimation.Close();
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::Chat)
	{
		ChatDrawerAnimation.Close();
	}
	else
	{
		WindowAnimation.Close();
	}
	ApplyWindowAnimation();
	AdvanceWindowAnimation(0.0f);
}

void URokUnityFeatureScreenWidget::HandleResearchMainMaskClicked()
{
	RequestResearchMainMaskClose();
}

void URokUnityFeatureScreenWidget::RequestResearchMainMaskClose()
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Research || !IsScreenOpen() ||
		bResearchMainMaskClosing ||
		(ResearchScreenWidget && ResearchScreenWidget->IsDetailVisible())) return;
	// UI.s_ResearchMain uses kTouchClose. Its mask invokes CloseUI with the
	// default non-force path, which plays root Animator Close from time zero.
	bResearchMainMaskClosing = true;
	PendingActionId = INDEX_NONE;
	WindowAnimation.Close();
	ApplyWindowAnimation();
	AdvanceWindowAnimation(0.0f);
}

#if WITH_DEV_AUTOMATION_TESTS
void URokUnityFeatureScreenWidget::FlushPendingActionForTesting()
{
	if (PendingActionId == INDEX_NONE)
	{
		return;
	}
	const int32 ActionId = PendingActionId;
	PendingActionId = INDEX_NONE;
	ExecuteIndexedAction(ActionId);
}
#endif

TSharedRef<SWidget> URokUnityFeatureScreenWidget::RebuildWidget()
{
	BuildWidgetTree();
	return Super::RebuildWidget();
}

void URokUnityFeatureScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(IsScreenOpen() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void URokUnityFeatureScreenWidget::NativeDestruct()
{
	CloseScreen();
	Super::NativeDestruct();
}

FReply URokUnityFeatureScreenWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (ActiveMode == ERokUnityFeatureScreenMode::Vip
		&& VipDailyPointHelpTipRoot
		&& InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FWidgetPath HitPath = FSlateApplication::Get().LocateWindowUnderMouse(
			InMouseEvent.GetScreenSpacePosition(),
			FSlateApplication::Get().GetInteractiveTopLevelWindows());
		const bool bInsideVipTip = HitPath.IsValid()
			&& HitPath.ContainsWidget(&VipDailyPointHelpTipRoot->TakeWidget().Get());
		if (!bInsideVipTip)
		{
			VipDailyPointHelpTipRoot->RemoveFromParent();
			VipDailyPointHelpTipRoot = nullptr;
			bVipDailyPointHelpVisible = false;
			// Consume this pointer-down so the same click cannot reach VIP claim/purchase controls.
			// Clicking the question icon again closes the tip and is not replayed as a second open.
			return FReply::Handled();
		}
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Mail
		&& MailRewardTooltipRoot
		&& InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Preview only runs for a real Slate widget hit. The Unity HelpTip leaves
		// itself open on an own-child hit, and closes before another UI target
		// handles the same click. Blank space has no hit path and stays open.
		const FWidgetPath HitPath = FSlateApplication::Get().LocateWindowUnderMouse(
			InMouseEvent.GetScreenSpacePosition(),
			FSlateApplication::Get().GetInteractiveTopLevelWindows());
		UWidget* BodyHit = GetWidgetFromName(TEXT("MailRewardTooltipBodyHit"));
		UWidget* ArrowHit = GetWidgetFromName(TEXT("MailRewardTooltipArrowHit"));
		const bool bInsideTip = HitPath.IsValid()
			&& ((BodyHit && HitPath.ContainsWidget(&BodyHit->TakeWidget().Get()))
				|| (ArrowHit && HitPath.ContainsWidget(&ArrowHit->TakeWidget().Get())));
		UWidget* ScreenBlocker = GetWidgetFromName(TEXT("FeatureScreenInputBlocker"));
		const bool bOnlyScreenBlocker = ScreenBlocker && HitPath.IsValid()
			&& HitPath.ContainsWidget(&ScreenBlocker->TakeWidget().Get());
		if (!bInsideTip && !bOnlyScreenBlocker)
		{
			MailRewardTooltipRoot->RemoveFromParent();
			MailRewardTooltipRoot = nullptr;
			ActiveMailRewardTooltipIndex = INDEX_NONE;
			MailRewardTooltipAnchor = FVector2D::ZeroVector;
		}
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

void URokUnityFeatureScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (ActiveMode == ERokUnityFeatureScreenMode::Hospital)
	{
		HospitalRefreshAccumulator += InDeltaTime;
		if (HospitalRefreshAccumulator >= 0.25f)
		{
			HospitalRefreshAccumulator = 0;
			RefreshHospital();
		}
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Mail)
	{
		RefreshBattleReportMail();
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Commander) UpdateCommanderSceneMotion();
	AdvanceTaskCompletedRowAnimation();
	if (ActiveMode == ERokUnityFeatureScreenMode::Expedition)
	{
		AdvanceExpeditionAnimation(InDeltaTime);
	}
	MailAttachmentPulse.Advance(InDeltaTime);
	UpdateExpandedDesignResolution(MyGeometry.GetLocalSize());
	if (ActiveMode == ERokUnityFeatureScreenMode::BuildCity && bBuildCityGuideNeedsScroll)
	{
		// The drawer can open during a level transition. Center the card once the expanded width is known.
		if (auto* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("BuildCityBuildingScroll"))))
		{
			const int32 Index = VisibleBuildCityTypeIds.IndexOfByKey(BuildCityGuideTypeId);
			double CardCenter = (Index + 0.5) * BuildCityCardStride;
			const FName TargetName(*FString::Printf(TEXT("BuildCityCardSize_%d"), BuildCityGuideTypeId));
			double PrecedingWidth = 0.0;
			for (const UWidget* Child : Scroll->GetAllChildren())
			{
				const auto* CardSize = Cast<USizeBox>(Child);
				if (!CardSize || !CardSize->IsWidthOverride() || CardSize->GetWidthOverride() <= 0.0f)
				{
					break;
				}
				const double CardWidth = CardSize->GetWidthOverride();
				if (CardSize->GetFName() == TargetName)
				{
					CardCenter = PrecedingWidth + CardWidth * 0.5;
					break;
				}
				PrecedingWidth += CardWidth;
			}

			const auto WidthOf = [](const UWidget* Child, double ParentWidth, double Fallback)
			{
				const auto* CanvasSlot = Child ? Cast<UCanvasPanelSlot>(Child->Slot) : nullptr;
				if (!CanvasSlot)
				{
					return Fallback;
				}
				const FAnchors Anchors = CanvasSlot->GetAnchors();
				const double Span = Anchors.Maximum.X - Anchors.Minimum.X;
				const FMargin Offsets = CanvasSlot->GetOffsets();
				const double Width = Span > 0.0 ? ParentWidth * Span - Offsets.Left - Offsets.Right : Offsets.Right;
				return Width > 0.0 ? Width : Fallback;
			};
			const double DefaultListWidth = CurrentExpandedDesignSize.X - BuildCityListLeft;
			const double MaskWidth = WidthOf(GetWidgetFromName(TEXT("BuildCityListMask")), CurrentExpandedDesignSize.X, DefaultListWidth);
			const double ListWidth = WidthOf(Scroll, MaskWidth, MaskWidth);
			Scroll->SetScrollOffset(FMath::Max(0.0, CardCenter - ListWidth * 0.5));
		}
		bBuildCityGuideNeedsScroll = false;
	}
	AdvanceWindowAnimation(InDeltaTime);
	// A newly queued Slate action starts this tip at t=0 after this advance.
	AdvanceWorldPositionMiddleTip(InDeltaTime);
	if (ActiveMode == ERokUnityFeatureScreenMode::TrainArmy)
	{
		// Unity starts Show after async prefab/assets finish loading. Do not
		// charge synchronous UMG/texture construction to the first visible frame.
		AdvanceTrainArmyDetailAnimation(bTrainArmyDetailFirstTick ? 0.0f : InDeltaTime);
		bTrainArmyDetailFirstTick = false;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::ArmyShow)
	{
		AdvanceArmyShowPanelAnimation(InDeltaTime);
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Commander)
	{
		// The first frame delta can include synchronous portrait loading. Unity
		// starts OpenNoArb after its async LoadFinish; don't spend entry time loading.
		AdvanceCommanderRosterAnimation(bCommanderRosterFirstTick ? 0.0f : InDeltaTime);
		bCommanderRosterFirstTick = false;
	}
	if (PendingActionId != INDEX_NONE)
	{
		const int32 ActionId = PendingActionId;
		PendingActionId = INDEX_NONE;
		ExecuteIndexedAction(ActionId);
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::ActivityCenter)
	{
		UpdateActivityCountdown(InDeltaTime);
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::SpeedUp)
	{
		SpeedUpRefreshAccumulator += InDeltaTime;
		if (SpeedUpRefreshAccumulator >= 0.25f)
		{
			SpeedUpRefreshAccumulator = 0.0f;
			UpdateSpeedUpQueuePresentation();
		}
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::WorkerQueue)
	{
		WorkerQueueRefreshAccumulator += InDeltaTime;
		if (WorkerQueueRefreshAccumulator >= 0.25f)
		{
			WorkerQueueRefreshAccumulator = 0.0f;
			UpdateWorkerQueuePresentation();
		}
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::Tasks)
	{
		TaskProjectionRefreshAccumulator += InDeltaTime;
		if (TaskProjectionRefreshAccumulator >= 0.25f)
		{
			TaskProjectionRefreshAccumulator = 0.0f;
			RefreshServerTaskPresentation();
			if (SelectedTaskPage == 1 && !bLastServerTaskAvailable)
			{
				RefreshTaskMainProjection();
			}
		}
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::ArmyStatistics
		|| ActiveMode == ERokUnityFeatureScreenMode::ArmyShow
		|| ActiveMode == ERokUnityFeatureScreenMode::TrainArmy)
	{
		ServerArmyRefreshAccumulator += InDeltaTime;
		if (ServerArmyRefreshAccumulator >= 0.25f)
		{
			ServerArmyRefreshAccumulator = 0.0f;
			RefreshServerArmyPresentation();
		}
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::Commander
		|| ActiveMode == ERokUnityFeatureScreenMode::Items)
	{
		ServerPlayerAssetsRefreshAccumulator += InDeltaTime;
		if (ServerPlayerAssetsRefreshAccumulator >= 0.25f)
		{
			ServerPlayerAssetsRefreshAccumulator = 0.0f;
			RefreshServerPlayerAssetsPresentation();
		}
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::Vip && !bVipTestFixtureState)
	{
		RefreshVipPresentation();
	}
}

void URokUnityFeatureScreenWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RokUnityFeatureScreenRoot"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (UClass* ShellClass = ResolveFeaturePageClass(TEXT("FeatureShell")))
	{
		if (auto* Shell = CreateWidget<URokUiPageWidget>(this, ShellClass))
		{
			auto* DesignSize = Shell->Get<USizeBox>(TEXT("FeatureExpandedDesignSize"));
			auto* DesignCanvas = Shell->Get<UCanvasPanel>(TEXT("FeatureExpandedDesignCanvas"));
			auto* Content = Shell->Get<UCanvasPanel>(TEXT("FeatureModeCanvas"));
			auto* Chrome = Shell->Get<UCanvasPanel>(TEXT("CommanderChrome"));
			auto* Scene = Shell->Get<UImage>(TEXT("CommanderSceneBackground"));
			auto* Blocker = Shell->Get<UButton>(TEXT("FeatureScreenInputBlocker"));
			const auto* ScaleBox = Shell->Get<UScaleBox>(TEXT("FeatureUnityExpandScaleBox"));
			const auto* Dimmer = Shell->Get<UBorder>(TEXT("FeatureScreenDimmer"));
			if (DesignSize && DesignCanvas && Content && Chrome && Scene && Blocker && ScaleBox && Dimmer
				&& ScaleBox->GetContent() == DesignSize && DesignSize->GetContent() == DesignCanvas
				&& Content->GetParent() == DesignCanvas && Chrome->GetParent() == DesignCanvas)
			{
				FeatureShellPage = Shell;
				ExpandedDesignSizeBox = DesignSize;
				ExpandedDesignCanvas = DesignCanvas;
				ModeCanvas = Content;
				CommanderChrome = Chrome;
				CommanderSceneImage = Scene;
				ExpandedDesignSizeBox->SetWidthOverride(CurrentExpandedDesignSize.X);
				ExpandedDesignSizeBox->SetHeightOverride(CurrentExpandedDesignSize.Y);
				Blocker->OnClicked.AddUniqueDynamic(this, &URokUnityFeatureScreenWidget::HandleResearchMainMaskClicked);

				Shell->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				auto* ShellSlot = RootCanvas->AddChildToCanvas(Shell);
				ShellSlot->SetAnchors(FAnchors(0, 0, 1, 1));
				ShellSlot->SetOffsets(FMargin(0));
				return;
			}
		}
	}

	UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FeatureScreenDimmer"));
	Dimmer->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.74f));
	Dimmer->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* DimmerSlot = RootCanvas->AddChildToCanvas(Dimmer);
	DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	DimmerSlot->SetOffsets(FMargin(0.0f));
	DimmerSlot->SetZOrder(0);

	CommanderSceneImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("CommanderSceneBackground"));
	CommanderSceneImage->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* SceneSlot = RootCanvas->AddChildToCanvas(CommanderSceneImage);
	SceneSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	SceneSlot->SetOffsets(FMargin(0));
	SceneSlot->SetZOrder(1);

	UButton* InputBlocker = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("FeatureScreenInputBlocker"));
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FButtonStyle BlockerStyle;
	BlockerStyle.SetNormal(NoBrush);
	BlockerStyle.SetHovered(NoBrush);
	BlockerStyle.SetPressed(NoBrush);
	BlockerStyle.SetDisabled(NoBrush);
	BlockerStyle.SetNormalPadding(FMargin(0.0f));
	BlockerStyle.SetPressedPadding(FMargin(0.0f));
	InputBlocker->SetStyle(BlockerStyle);
	InputBlocker->OnClicked.AddUniqueDynamic(this,
		&URokUnityFeatureScreenWidget::HandleResearchMainMaskClicked);
	UCanvasPanelSlot* BlockerSlot = RootCanvas->AddChildToCanvas(InputBlocker);
	BlockerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BlockerSlot->SetOffsets(FMargin(0.0f));
	BlockerSlot->SetZOrder(1);

	UScaleBox* ExpandScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("FeatureUnityExpandScaleBox"));
	ExpandScaleBox->SetStretch(EStretch::ScaleToFit);
	ExpandScaleBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ExpandScaleBox->SetStretchDirection(EStretchDirection::Both);
	UCanvasPanelSlot* ScaleSlot = RootCanvas->AddChildToCanvas(ExpandScaleBox);
	ScaleSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	ScaleSlot->SetOffsets(FMargin(0.0f));
	ScaleSlot->SetZOrder(2);

	ExpandedDesignSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("FeatureExpandedDesignSize"));
	ExpandedDesignSizeBox->SetWidthOverride(CurrentExpandedDesignSize.X);
	ExpandedDesignSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ExpandedDesignSizeBox->SetHeightOverride(CurrentExpandedDesignSize.Y);
	ExpandScaleBox->SetContent(ExpandedDesignSizeBox);

	ExpandedDesignCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FeatureExpandedDesignCanvas"));
	ExpandedDesignSizeBox->SetContent(ExpandedDesignCanvas);
	ExpandedDesignCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	ModeCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FeatureModeCanvas"));
	ModeCanvas->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UCanvasPanelSlot* ModeSlot = ExpandedDesignCanvas->AddChildToCanvas(ModeCanvas);
	ModeSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	ModeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ModeSlot->SetPosition(FVector2D::ZeroVector);
	ModeSlot->SetSize(FVector2D(UnityReferenceWidth, UnityReferenceHeight));

	CommanderChrome = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CommanderChrome"));
	CommanderChrome->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UCanvasPanelSlot* ChromeSlot = ExpandedDesignCanvas->AddChildToCanvas(CommanderChrome);
	ChromeSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	ChromeSlot->SetOffsets(FMargin(0));
	ChromeSlot->SetZOrder(3);
}

UWidget* URokUnityFeatureScreenWidget::GetWidgetFromName(const FName& Name) const
{
	if (FeedbackText && Name == FeedbackText->GetFName())
	{
		return FeedbackText;
	}
	if (ActiveModePage)
	{
		if (UWidget* Widget = RokUiFind::Find(ActiveModePage, Name))
		{
			return Widget;
		}
	}
	if (ModeCanvas)
	{
		for (const UWidget* Child : ModeCanvas->GetAllChildren())
		{
			const auto* Page = Cast<URokUiPageWidget>(Child);
			if (Page && Page != ActiveModePage.Get())
			{
				if (UWidget* Widget = RokUiFind::Find(Page, Name))
				{
					return Widget;
				}
			}
		}
	}
	for (const UCanvasPanel* Overlay : {VipDailyPointHelpTipRoot.Get(), MailRewardTooltipRoot.Get()})
	{
		if (!Overlay) continue;
		for (const UWidget* Child : Overlay->GetAllChildren())
		{
			if (const auto* Page = Cast<URokUiPageWidget>(Child))
			{
				if (UWidget* Found = RokUiFind::Find(Page, Name))
				{
					return Found;
				}
			}
		}
	}
	if (FeatureShellPage)
	{
		if (UWidget* Found = RokUiFind::Find(FeatureShellPage, Name))
		{
			return Found;
		}
	}
	return Super::GetWidgetFromName(Name);
}

UClass* URokUnityFeatureScreenWidget::ResolveFeaturePageClass(FName Name)
{
	if (URokUiSettings::IsLegacyForced())
	{
		return nullptr;
	}
	const FRokUiPageEntry* Entry = GetDefault<URokUiSettings>()->FeaturePages.FindByPredicate(
		[Name](const FRokUiPageEntry& Candidate) { return Candidate.Page == Name; });
	return Entry ? URokUiSettings::Resolve(Entry->Class) : nullptr;
}

URokUiPageWidget* URokUnityFeatureScreenWidget::MountModePage(FName PageName, bool bBindFrameClose, int32 ZOrder)
{
	if (PageName.IsNone())
	{
		PageName = FName(*StaticEnum<ERokUnityFeatureScreenMode>()->GetNameStringByValue(static_cast<int64>(ActiveMode)));
	}
	UClass* PageClass = ResolveFeaturePageClass(PageName);
	URokUiPageWidget* Page = PageClass ? CreateWidget<URokUiPageWidget>(this, PageClass) : nullptr;
	if (!Page)
	{
		return nullptr;
	}
	ActiveModePage = Page;
	Page->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UCanvasPanelSlot* PageSlot = ModeCanvas->AddChildToCanvas(Page);
	PageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
	PageSlot->SetOffsets(FMargin(0));
	PageSlot->SetZOrder(ZOrder);
	if (bBindFrameClose)
	{
		BindPageAction(*ActiveModePage, TEXT("FeatureFrameClose"), 1);
	}
	return ActiveModePage;
}

URokUiPageWidget* URokUnityFeatureScreenWidget::MountPageItem(
	UCanvasPanel* Parent, UClass* ItemClass, const TCHAR* LegacyPrefix, int32 Index)
{
	URokUiPageWidget* Item = Parent ? CreateWidget<URokUiPageWidget>(this, ItemClass) : nullptr;
	if (Item)
	{
		Item->LegacyPrefix = LegacyPrefix;
		Item->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
		Item->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		UCanvasPanelSlot* ItemSlot = Parent->AddChildToCanvas(Item);
		ItemSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		ItemSlot->SetOffsets(FMargin(0));
	}
	return Item;
}

void URokUnityFeatureScreenWidget::BindPageAction(const URokUiPageWidget& Page, FName Name, int32 ActionId)
{
	if (URokUnityIndexedButton* Button = Page.Get<URokUnityIndexedButton>(Name))
	{
		Button->ConfigureAction(ActionId);
		Button->OnIndexedClicked.AddUObject(this, &URokUnityFeatureScreenWidget::HandleIndexedAction);
	}
}

bool URokUnityFeatureScreenWidget::ExportCurrentModeSpec(const FString& Filename, FString& OutError) const
{
	if (!IsScreenOpen() || !ModeCanvas)
	{
		OutError = TEXT("No feature screen is open");
		return false;
	}
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("RokUiExportFeatureShell")))
	{
		const UWidget* Root = FeatureShellPage ? FeatureShellPage->GetRootWidget() : GetRootWidget();
		return RokUiSpec::ExportChildren(Cast<UPanelWidget>(Root), Filename, TEXT("FeatureShell"), OutError);
	}
#endif
	const FString Label = StaticEnum<ERokUnityFeatureScreenMode>()->GetNameStringByValue(static_cast<int64>(ActiveMode));
	return RokUiSpec::ExportChildren(ModeCanvas, Filename, Label, OutError);
}

void URokUnityFeatureScreenWidget::RebuildModeContent(bool bOpening)
{
	if (!ModeCanvas)
	{
		return;
	}
	ResetTaskCompletedRowAnimation();
	if (bOpening) bResearchMainMaskClosing = false;

	if (ActiveMode != ERokUnityFeatureScreenMode::TrainArmy && TrainArmyAnimatedModel)
	{
		TrainArmyAnimatedModel->ResetModel(); TrainArmyAnimatedModel = nullptr;
	}
	HelpWidget = nullptr;
	ResearchScreenWidget = nullptr;
	MailAttachmentPulse.BeginRebuild();
	URokUiAnimatedButton::ResetAnimationsInHierarchy(ModeCanvas);
	TrainArmyTipLayer = nullptr;
	MailRewardTooltipRoot = nullptr;
	VipDailyPointHelpTipRoot = nullptr;
	ActiveMailRewardTooltipIndex = INDEX_NONE;
	MailRewardTooltipAnchor = FVector2D::ZeroVector;
	TrainArmyAnimatedChildren.Reset();
	ActiveModePage = nullptr;
	ModeCanvas->ClearChildren();
	if (CommanderSceneImage) CommanderSceneImage->SetVisibility(ESlateVisibility::Collapsed);
	if (CommanderChrome) CommanderChrome->ClearChildren();
	CommanderRosterAnimationWidgets.Reset();
	CommanderDataAnimationWidgets.Reset();
	ArmyShowPanelAnimationWidgets.Reset();
	if (ActiveMode != ERokUnityFeatureScreenMode::Commander)
	{
		StopCommanderSceneMotion();
		CommanderAnimatedPortrait = nullptr;
		CommanderAnimatedPortraitHeroId = INDEX_NONE;
	}
	ChatContentCanvas = nullptr;
	FeedbackText = nullptr;
	WorldPositionMiddleTipRoot = nullptr;
	WorldPositionMiddleTipBackground = nullptr;
	WorldPositionMiddleTipText = nullptr;
	WorldPositionMiddleTipElapsedSeconds = -1.0f;
	ActivityCountdownText = nullptr;
	ServerSpeedUpConfirmation = nullptr;
	ServerSpeedUpConfirmIndex = INDEX_NONE;
	SpeedUpQueueDescriptionText = nullptr;
	SpeedUpQueueProgress = nullptr;
	SpeedUpQueueProgressText = nullptr;
	WorkerQueuePrimaryTitleText = nullptr;
	WorkerQueueRuntimeStateText = nullptr;
	WorkerQueuePrimaryProgress = nullptr;
	WorkerQueuePrimaryProgressText = nullptr;
	WorkerQueueSecondaryTitleText = nullptr;
	WorkerQueueSecondaryNoteText = nullptr;
	WorkerQueueSecondaryProgress = nullptr;
	WorkerQueueSecondaryProgressText = nullptr;
	ChatInputTextBox = nullptr;
	WorldPositionXInput = nullptr;
	WorldPositionYInput = nullptr;
	ResourceSearchMonsterTip = nullptr;
	ResourceSearchGuide = nullptr;
	ResourceSearchSlider = nullptr;
	ResourceSearchSliderFill = nullptr;
	ResourceSearchLevelValueText = nullptr;
	if (UBorder* Dimmer = Cast<UBorder>(GetWidgetFromName(TEXT("FeatureScreenDimmer"))))
	{
		Dimmer->SetBrushColor(FLinearColor(0, 0, 0,
			ActiveMode == ERokUnityFeatureScreenMode::Research ? 0.8f : 0.74f));
		Dimmer->SetVisibility(
			(ActiveMode == ERokUnityFeatureScreenMode::ResourceSearch || ActiveMode == ERokUnityFeatureScreenMode::Chat)
				? ESlateVisibility::Collapsed
				: ESlateVisibility::HitTestInvisible);
	}
	if (UWidget* InputBlocker = GetWidgetFromName(TEXT("FeatureScreenInputBlocker")))
	{
		// Unity UI.s_chat uses EnumMaskStatus.kNone: uncovered HUD remains
		// hit-testable. Chat has only bounds-matched panel shields below.
		InputBlocker->SetVisibility(ActiveMode == ERokUnityFeatureScreenMode::Chat
			? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (UCanvasPanelSlot* ModeSlot = Cast<UCanvasPanelSlot>(ModeCanvas->Slot))
	{
		if (ActiveMode == ERokUnityFeatureScreenMode::BuildCity)
		{
			ModeSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			ModeSlot->SetAlignment(FVector2D::ZeroVector);
			ModeSlot->SetOffsets(FMargin(0.0f));
		}
		else
		{
			const float AnchorX = ActiveMode == ERokUnityFeatureScreenMode::Chat ? 0.0f : 0.5f;
			ModeSlot->SetAnchors(FAnchors(AnchorX, 0.5f));
			ModeSlot->SetAlignment(FVector2D(AnchorX, 0.5f));
			ModeSlot->SetPosition(FVector2D::ZeroVector);
			ModeSlot->SetSize(FVector2D(UnityReferenceWidth, UnityReferenceHeight));
		}
	}
	switch (ActiveMode)
	{
	case ERokUnityFeatureScreenMode::ActivityCenter:
		BuildActivityCenter();
		break;
	case ERokUnityFeatureScreenMode::Mail:
		BuildMail();
		break;
	case ERokUnityFeatureScreenMode::Research:
		BuildResearch();
		break;
	case ERokUnityFeatureScreenMode::Tasks:
		BuildTasks();
		break;
	case ERokUnityFeatureScreenMode::Commander:
		BuildCommander();
		break;
	case ERokUnityFeatureScreenMode::Items:
		BuildItems();
		break;
	case ERokUnityFeatureScreenMode::Alliance:
		BuildAlliance();
		break;
	case ERokUnityFeatureScreenMode::Vip:
		BuildVip();
		break;
	case ERokUnityFeatureScreenMode::Chat:
		BuildChat();
		break;
	case ERokUnityFeatureScreenMode::WorldPositionSearch:
		BuildWorldPositionSearch();
		break;
	case ERokUnityFeatureScreenMode::MapMarkers:
		BuildMapMarkers();
		break;
	case ERokUnityFeatureScreenMode::ResourceSearch:
		BuildResourceSearch();
		break;
	case ERokUnityFeatureScreenMode::Expedition:
		BuildExpedition();
		break;
	case ERokUnityFeatureScreenMode::GatherArmy:
		BuildArmyComposer();
		break;
	case ERokUnityFeatureScreenMode::GemShop:
		BuildGemShop();
		break;
	case ERokUnityFeatureScreenMode::BuildCity:
		BuildBuildCity();
		break;
	case ERokUnityFeatureScreenMode::WorkerQueue:
		BuildWorkerQueue();
		break;
	case ERokUnityFeatureScreenMode::SettingsHub:
		BuildSettingsHub();
		break;
	case ERokUnityFeatureScreenMode::GeneralSettings:
		BuildGeneralSettings();
		break;
	case ERokUnityFeatureScreenMode::NotificationSettings:
		BuildNotificationSettings();
		break;
	case ERokUnityFeatureScreenMode::Help:
		BuildHelp();
		break;
	case ERokUnityFeatureScreenMode::LanguageSettings:
		BuildLanguageSettings();
		break;
	case ERokUnityFeatureScreenMode::AccountSettings:
		BuildAccountSettings();
		break;
	case ERokUnityFeatureScreenMode::ArmyStatistics:
		BuildArmyStatistics();
		break;
	case ERokUnityFeatureScreenMode::Store:
		BuildStore();
		break;
	case ERokUnityFeatureScreenMode::TrainArmy:
		BuildTrainArmy();
		break;
	case ERokUnityFeatureScreenMode::SpeedUp:
		BuildSpeedUp();
		break;
	case ERokUnityFeatureScreenMode::Hospital:
		BuildHospital();
		break;
	case ERokUnityFeatureScreenMode::CityManager:
		BuildCityManager();
		break;
	case ERokUnityFeatureScreenMode::ArmyShow:
		BuildArmyShow();
		break;
	case ERokUnityFeatureScreenMode::Charge:
		BuildCharge();
		break;
	default:
		break;
	}
	MailAttachmentPulse.EndRebuild();
	if (ActiveMode != ERokUnityFeatureScreenMode::GatherArmy) BuildFeedback();
	UpdateWindowAnimationForMode(bOpening);
}

URokUnityFeatureScreenWidget::EAllianceAnimatedRoot
URokUnityFeatureScreenWidget::ResolveAllianceAnimatedRoot() const
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Alliance
		|| bAllianceOfficeReferenceOpen || bAllianceTechnologyReferenceOpen
		|| bAllianceTerritoryReferenceOpen || bAllianceRankingReferenceOpen)
	{
		return EAllianceAnimatedRoot::None;
	}
	if (bAllianceShopReferenceOpen) return EAllianceAnimatedRoot::Shop;
	if (bAllianceGiftReferenceOpen) return EAllianceAnimatedRoot::Gift;
	if (AlliancePreviewState == 1) return EAllianceAnimatedRoot::Join;
	if (AlliancePreviewState == 3)
	{
		return bAllianceFlagEditorOpen ? EAllianceAnimatedRoot::Flag : EAllianceAnimatedRoot::None;
	}
	return AlliancePreviewState == 2
		? EAllianceAnimatedRoot::Main : EAllianceAnimatedRoot::None;
}

bool URokUnityFeatureScreenWidget::TryStartAllianceRootClose(int32 ActionId)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Alliance
		|| PendingAllianceRootCloseActionId != INDEX_NONE) return false;
	const bool bBack = (AllianceAnimatedRoot == EAllianceAnimatedRoot::Join && ActionId == 621)
		|| (AllianceAnimatedRoot == EAllianceAnimatedRoot::Gift && ActionId == 686)
		|| (AllianceAnimatedRoot == EAllianceAnimatedRoot::Shop && ActionId == 690)
		|| (AllianceAnimatedRoot == EAllianceAnimatedRoot::Flag
			&& (ActionId == 19010 || ActionId == 19011));
	if (!bBack || (ActionId == 19011
		&& !RokAllianceFlagLocalPreview::IsValidDraft(AllianceFlagEditDraft))) return false;
	PendingAllianceRootCloseActionId = ActionId;
	WindowAnimation.Close();
	ApplyWindowAnimation();
	return true;
}

void URokUnityFeatureScreenWidget::UpdateWindowAnimationForMode(bool bOpening)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Alliance)
	{
		if (AllianceAnimatedRoot != EAllianceAnimatedRoot::None
			|| PendingAllianceRootCloseActionId != INDEX_NONE)
		{
			WindowAnimation.Reset();
			AnimatedMode = ERokUnityFeatureScreenMode::None;
		}
		AllianceAnimatedRoot = EAllianceAnimatedRoot::None;
		PendingAllianceRootCloseActionId = INDEX_NONE;
		bDispatchingAllianceRootCloseAction = false;
	}
	else
	{
		const EAllianceAnimatedRoot NextRoot = ResolveAllianceAnimatedRoot();
		if (NextRoot == EAllianceAnimatedRoot::None)
		{
			WindowAnimation.Reset();
			AllianceAnimatedRoot = EAllianceAnimatedRoot::None;
			AnimatedMode = ActiveMode;
			ApplyWindowAnimation();
			return;
		}
		if (NextRoot != AllianceAnimatedRoot)
		{
			AllianceAnimatedRoot = NextRoot;
			WindowAnimation.Open(ERokUiWindowAnimationStyle::Type1);
			AnimatedMode = ActiveMode;
			ApplyWindowAnimation();
			return;
		}
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::BuildCity && AnimatedMode != ActiveMode)
	{
		BuildCityEntranceTime = 0;
		BuildCityCardAnimationTime = 0;
	}
	if (ActiveMode != ERokUnityFeatureScreenMode::BuildCity) BuildCityAnimatedRoot = nullptr;
	if (ActiveMode == ERokUnityFeatureScreenMode::ResourceSearch)
	{
		if (AnimatedMode != ActiveMode || (bOpening && ResourceSearchAnimation.IsClosing())) ResourceSearchAnimation.Open();
		WindowAnimation.Reset();
		ChatDrawerAnimation.Reset();
		AnimatedMode = ActiveMode;
		ApplyWindowAnimation();
		return;
	}
	ResourceSearchAnimation.Reset();
	if (ActiveMode == ERokUnityFeatureScreenMode::Chat)
	{
		if (AnimatedMode != ActiveMode)
		{
			WindowAnimation.Reset();
			ChatDrawerAnimation.Reset();
			ChatContactAnimation.Reset();
			ChatDrawerAnimation.Open();
		}
		else if (bOpening && ChatDrawerAnimation.IsClosing())
		{
			ChatDrawerAnimation.Open();
		}
		AnimatedMode = ActiveMode;
		ApplyWindowAnimation();
		return;
	}
	ChatDrawerAnimation.Reset();
	ChatContactAnimation.Reset();
	if (AnimatedMode != ActiveMode || (bOpening && WindowAnimation.IsClosing()))
	{
		// Exact prefab root Animator mappings generated by
		// BuildRokUnityUiAnimationManifest.py, not guessed from frame artwork.
		ERokUiWindowAnimationStyle Style = ERokUiWindowAnimationStyle::None;
		switch (ActiveMode)
		{
		case ERokUnityFeatureScreenMode::ActivityCenter:
		case ERokUnityFeatureScreenMode::Mail:
		case ERokUnityFeatureScreenMode::Tasks:
		case ERokUnityFeatureScreenMode::Items:
		case ERokUnityFeatureScreenMode::Alliance:
		case ERokUnityFeatureScreenMode::Vip:
		case ERokUnityFeatureScreenMode::WorkerQueue:
		case ERokUnityFeatureScreenMode::SettingsHub:
		case ERokUnityFeatureScreenMode::GeneralSettings:
		case ERokUnityFeatureScreenMode::NotificationSettings:
		case ERokUnityFeatureScreenMode::Help:
		case ERokUnityFeatureScreenMode::LanguageSettings:
		case ERokUnityFeatureScreenMode::AccountSettings:
		case ERokUnityFeatureScreenMode::ArmyStatistics:
		case ERokUnityFeatureScreenMode::Store:
		case ERokUnityFeatureScreenMode::TrainArmy:
		case ERokUnityFeatureScreenMode::SpeedUp:
		case ERokUnityFeatureScreenMode::Hospital:
		case ERokUnityFeatureScreenMode::CityManager:
		case ERokUnityFeatureScreenMode::ArmyShow:
		case ERokUnityFeatureScreenMode::Research:
			Style = ERokUiWindowAnimationStyle::Type1;
			break;
		case ERokUnityFeatureScreenMode::Expedition:
		case ERokUnityFeatureScreenMode::Charge:
			Style = ERokUiWindowAnimationStyle::Interface;
			break;
		default:
			break;
		}
		WindowAnimation.Open(Style);
		AnimatedMode = ActiveMode;
	}
	ApplyWindowAnimation();
}

void URokUnityFeatureScreenWidget::ApplyWindowAnimation()
{
	if (ActiveMode == ERokUnityFeatureScreenMode::BuildCity && BuildCityAnimatedRoot)
	{
		// UA_BuildingCity_On animates child rect, not the input mask.
		const float T = FMath::Clamp(BuildCityEntranceTime * 3.0f, 0.0f, 1.0f);
		const float Alpha = T * T * (3.0f - 2.0f * T);
		BuildCityAnimatedRoot->SetRenderTranslation(FVector2D(0, 100.0f * (1 - Alpha)));
		BuildCityAnimatedRoot->SetRenderOpacity(Alpha);
		for (int32 Index = 0; Index < BuildCityAnimatedCards.Num(); ++Index)
		{
			BuildCityAnimatedCards[Index]->SetRenderScale(FVector2D(
				RokBuildCityCardAnimation::Scale(BuildCityCardAnimationTime, BuildCityCardSourceIndices[Index])));
		}
	}
	if (ModeCanvas && ActiveMode == ERokUnityFeatureScreenMode::ResourceSearch)
	{
		ModeCanvas->SetRenderScale(FVector2D(1));
		ModeCanvas->SetRenderTranslation(FVector2D::ZeroVector);
		ModeCanvas->SetRenderOpacity(1);
		ModeCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (ResourceSearchDockRoot) ResourceSearchDockRoot->SetRenderTranslation(FVector2D(0, ResourceSearchAnimation.DockTranslationY));
		if (ResourceSearchPopupRoot) ResourceSearchPopupRoot->SetRenderScale(FVector2D(ResourceSearchAnimation.PopupScale));
		if (ResourceSearchGuide)
		{
			const double Offset = bFreezeResourceSearchGuideForCapture ? 0.0 : 5.0 * FMath::Sin(FPlatformTime::Seconds() * 5.0);
			ResourceSearchGuide->SetRenderTranslation(FVector2D(0, Offset));
		}
		return;
	}
	if (ModeCanvas && ActiveMode == ERokUnityFeatureScreenMode::Chat)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ModeCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ModeCanvas->SetRenderScale(FVector2D(1.0f));
		ModeCanvas->SetRenderTranslation(FVector2D(ChatDrawerAnimation.GetTranslationX(), 0.0f));
		ModeCanvas->SetRenderOpacity(ChatDrawerAnimation.Opacity);
		if (ChatContentCanvas)
		{
			ChatContentCanvas->SetRenderTranslation(FVector2D(ChatContactAnimation.TranslationX, 0.0f));
			ChatContentCanvas->SetVisibility(ChatDrawerAnimation.IsAnimating()
				? ESlateVisibility::HitTestInvisible : ESlateVisibility::SelfHitTestInvisible);
		}
		for (const TCHAR* Name : { TEXT("ChatAnimationInputShield"), TEXT("ChatCloseInputShield") })
		{
			if (UWidget* Shield = RokUiFind::Find(this, Name))
			{
				Shield->SetRenderTranslation(FVector2D(ChatContactAnimation.TranslationX, 0.0f));
				Shield->SetVisibility(ChatDrawerAnimation.IsAnimating()
					? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			}
		}
		return;
	}
	if (ModeCanvas)
	{
		// Animator.Play("Close", -1, 0) replaces an in-progress Open and uses
		// the Close clip's first key (scale/alpha 1), rather than interpolating
		// from the interrupted Open sample as the shared helper normally does.
		const bool bExactSourceClose = bResearchMainMaskClosing
			|| (ActiveMode == ERokUnityFeatureScreenMode::Alliance
				&& AllianceAnimatedRoot != EAllianceAnimatedRoot::None
				&& WindowAnimation.IsClosing());
		const FRokUiWindowAnimationSample Sample = bExactSourceClose
			? FRokUiWindowAnimation::Evaluate(ERokUiWindowAnimationStyle::Type1,
				true, WindowAnimation.GetElapsedSeconds())
			: WindowAnimation.GetSample();
		ModeCanvas->SetRenderTranslation(FVector2D::ZeroVector);
		ModeCanvas->SetRenderScale(FVector2D(Sample.Scale));
		ModeCanvas->SetRenderOpacity(Sample.Opacity);
		ModeCanvas->SetVisibility(WindowAnimation.IsAnimating() && !bResearchMainMaskClosing
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::SelfHitTestInvisible);
	}
}

#if !UE_BUILD_SHIPPING
void URokUnityFeatureScreenWidget::SampleBuildCityAnimationForCapture(float ElapsedSeconds)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::BuildCity) return;
	bFreezeBuildCityAnimationForCapture = true;
	BuildCityCardAnimationTime = FMath::Clamp(ElapsedSeconds, 0.0f, 10.0f);
	BuildCityEntranceTime = FMath::Min(BuildCityCardAnimationTime, 1.0f / 3.0f);
	ApplyWindowAnimation();
	for (int32 Index = 0; Index < BuildCityAnimatedCards.Num(); ++Index)
	{
		if (const UCanvasPanel* Card = BuildCityAnimatedCards[Index])
			UE_LOG(LogTemp, Log, TEXT("Rok BuildCity animation capture: %s sourceIndex=%d time=%.4f scale=%.4f"),
				*Card->GetName(), BuildCityCardSourceIndices[Index], BuildCityCardAnimationTime, Card->GetRenderTransform().Scale.X);
	}
}
#endif

void URokUnityFeatureScreenWidget::AdvanceWindowAnimation(float DeltaSeconds)
{
	if (ActiveMode == ERokUnityFeatureScreenMode::BuildCity)
	{
#if !UE_BUILD_SHIPPING
		if (bFreezeBuildCityAnimationForCapture) return;
#endif
		BuildCityEntranceTime = FMath::Min(1.0f / 3.0f, BuildCityEntranceTime + FMath::Max(DeltaSeconds, 0.0f));
		BuildCityCardAnimationTime = FMath::Min(10.0f, BuildCityCardAnimationTime + FMath::Max(DeltaSeconds, 0.0f));
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::ResourceSearch)
	{
		const bool bClosed = ResourceSearchAnimation.Advance(DeltaSeconds);
		ApplyWindowAnimation();
		if (bClosed) CloseScreen();
		return;
	}
	const bool bChat = ActiveMode == ERokUnityFeatureScreenMode::Chat;
	const bool bFinishedClose = bChat
		? ChatDrawerAnimation.Advance(DeltaSeconds) : WindowAnimation.Advance(DeltaSeconds);
	if (bChat)
	{
		ChatContactAnimation.Advance(DeltaSeconds);
	}
	ApplyWindowAnimation();
	if (bFinishedClose)
	{
		if (ActiveMode == ERokUnityFeatureScreenMode::Alliance
			&& PendingAllianceRootCloseActionId != INDEX_NONE)
		{
			const int32 ActionId = PendingAllianceRootCloseActionId;
			PendingAllianceRootCloseActionId = INDEX_NONE;
			bDispatchingAllianceRootCloseAction = true;
#if WITH_DEV_AUTOMATION_TESTS
			++AllianceRootCloseDispatchCountForAudit;
			AllianceRootCloseLastActionForAudit = ActionId;
#endif
			ExecuteIndexedAction(ActionId);
			bDispatchingAllianceRootCloseAction = false;
			return;
		}
		CloseScreen();
	}
}

#include "RokFeatureChat.inl"

void URokUnityFeatureScreenWidget::BuildWorldPositionSearch()
{
	if (const URokUiPageWidget* Page = MountModePage())
	{
		WorldPositionXInput = Page->Get<UEditableTextBox>(TEXT("WorldPositionXInput"));
		WorldPositionYInput = Page->Get<UEditableTextBox>(TEXT("WorldPositionYInput"));
		if (WorldPositionXInput)
		{
			WorldPositionXInput->SetText(FText::FromString(FString::FromInt(SearchCoordinateX)));
			WorldPositionXInput->OnTextCommitted.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleWorldPositionXCommitted);
		}
		if (WorldPositionYInput)
		{
			WorldPositionYInput->SetText(FText::FromString(FString::FromInt(SearchCoordinateY)));
			WorldPositionYInput->OnTextCommitted.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleWorldPositionYCommitted);
		}
		BindPageAction(*Page, TEXT("WorldPositionMinus_1"), 850);
		BindPageAction(*Page, TEXT("WorldPositionPlus_1"), 851);
		BindPageAction(*Page, TEXT("WorldPositionMinus_2"), 852);
		BindPageAction(*Page, TEXT("WorldPositionPlus_2"), 853);
		BindPageAction(*Page, TEXT("WorldPositionGo"), 854);
		WorldPositionMiddleTipRoot = Page->Get<UCanvasPanel>(TEXT("WorldPositionMiddleTipRoot"));
		WorldPositionMiddleTipBackground = Page->Get<UImage>(TEXT("WorldPositionMiddleTipBackground"));
		WorldPositionMiddleTipText = Page->Get<UTextBlock>(TEXT("WorldPositionMiddleTipText"));
		return;
	}
	BuildSplitWindowFrame(
		CapturePoint(360.0f, 190.0f),
		FVector2D(720.0f, 430.0f),
		NSLOCTEXT("RokWorldPosition", "Title", "坐标搜索"),
		CapturePoint(1020.0f, 186.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("WorldPositionContentShade"),
			FLinearColor(0.04f, 0.05f, 0.05f, 0.98f)),
		CapturePoint(384.0f, 268.0f),
		FVector2D(672.0f, 318.0f),
		2);

	static const FText FieldLabels[] = {
		NSLOCTEXT("RokWorldPosition", "Kingdom", "王国"),
		FText::FromString(TEXT("X")),
		FText::FromString(TEXT("Y"))};
	const int32 Values[] = {
		1,
		SearchCoordinateX,
		SearchCoordinateY};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UTextBlock* Label = MakeScreenText(
			*FString::Printf(TEXT("WorldPositionLabel_%d"), Index),
			22,
			FLinearColor(0.9f, 0.84f, 0.65f, 1.0f),
			true);
		Label->SetText(FieldLabels[Index]);
		PlaceOnCanvas(
			ModeCanvas,
			Label,
			CapturePoint(424.0f, 296.0f + Index * 72.0f),
			FVector2D(110.0f, 40.0f),
			5);
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				*FString::Printf(TEXT("WorldPositionValueShade_%d"), Index),
				FLinearColor(0.015f, 0.02f, 0.02f, 1.0f)),
			CapturePoint(535.0f, 288.0f + Index * 72.0f),
			FVector2D(350.0f, 54.0f),
			4);
		if (Index == 0)
		{
			UTextBlock* ValueText = MakeScreenText(
				TEXT("WorldPositionKingdomValue"),
				24,
				FLinearColor::White,
				true,
				ETextJustify::Center);
			// A local preview constant, not PlayerProxy.GetGameNode() account identity.
			ValueText->SetText(FText::FromString(TEXT("1（本地预览）")));
			PlaceOnCanvas(
				ModeCanvas,
				ValueText,
				CapturePoint(535.0f, 296.0f),
				FVector2D(350.0f, 40.0f),
				5);
		}
		else
		{
			UEditableTextBox* CoordinateInput =
				WidgetTree->ConstructWidget<UEditableTextBox>(
					UEditableTextBox::StaticClass(),
					Index == 1
						? TEXT("WorldPositionXInput")
						: TEXT("WorldPositionYInput"));
			CoordinateInput->SetText(
				FText::FromString(FString::FromInt(Values[Index])));
			FEditableTextBoxStyle CoordinateInputStyle =
				FEditableTextBoxStyle::GetDefault();
			CoordinateInputStyle
				.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24))
				.SetForegroundColor(FLinearColor::White)
				.SetFocusedForegroundColor(FLinearColor::White)
				.SetBackgroundColor(
					FLinearColor(0.015f, 0.02f, 0.02f, 1.0f))
				.SetPadding(FMargin(8.0f, 4.0f));
			CoordinateInput->SetWidgetStyle(CoordinateInputStyle);
			CoordinateInput->SetJustification(ETextJustify::Center);
			CoordinateInput->SetSelectAllTextWhenFocused(true);
			CoordinateInput->SetClearKeyboardFocusOnCommit(false);
			if (Index == 1)
			{
				WorldPositionXInput = CoordinateInput;
				CoordinateInput->OnTextCommitted.AddDynamic(
					this,
					&URokUnityFeatureScreenWidget::HandleWorldPositionXCommitted);
			}
			else
			{
				WorldPositionYInput = CoordinateInput;
				CoordinateInput->OnTextCommitted.AddDynamic(
					this,
					&URokUnityFeatureScreenWidget::HandleWorldPositionYCommitted);
			}
			PlaceOnCanvas(
				ModeCanvas,
				CoordinateInput,
				CapturePoint(
					539.0f,
					292.0f + Index * 72.0f),
				FVector2D(342.0f, 46.0f),
				5);
		}

		if (Index > 0)
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeActionButton(
					*FString::Printf(TEXT("WorldPositionMinus_%d"), Index),
					Index == 1 ? 850 : 852,
					CommonAtlasPath,
					MiniBlueButtonRect,
					CommonAtlasSize,
					FVector2D(64.0f, 54.0f),
					FText::FromString(TEXT("−")),
					24,
					FLinearColor::White,
					ESlateBrushDrawType::Box,
					MiniBlueMargin),
				CapturePoint(902.0f, 288.0f + Index * 72.0f),
				FVector2D(64.0f, 54.0f),
				6);
			PlaceOnCanvas(
				ModeCanvas,
				MakeActionButton(
					*FString::Printf(TEXT("WorldPositionPlus_%d"), Index),
					Index == 1 ? 851 : 853,
					CommonAtlasPath,
					MiniGreenButtonRect,
					CommonAtlasSize,
					FVector2D(64.0f, 54.0f),
					FText::FromString(TEXT("+")),
					24,
					FLinearColor::White,
					ESlateBrushDrawType::Box,
					MiniGreenMargin),
				CapturePoint(972.0f, 288.0f + Index * 72.0f),
				FVector2D(64.0f, 54.0f),
				6);
		}
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("WorldPositionGo"),
			854,
			CommonAtlasPath,
			LargeBlueButtonRect,
			CommonAtlasSize,
			FVector2D(274.0f, 74.0f),
			NSLOCTEXT("RokWorldPosition", "Go", "前往"),
			27,
			FLinearColor(0.02f, 0.17f, 0.29f, 1.0f),
			ESlateBrushDrawType::Box,
			FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		CapturePoint(583.0f, 500.0f),
		FVector2D(274.0f, 74.0f),
		7);
}

void URokUnityFeatureScreenWidget::BuildMapMarkers()
{
	UClass* RowClass = ResolveFeaturePageClass(TEXT("MapMarkerRow"));
	UClass* SelectedRowClass = ResolveFeaturePageClass(TEXT("MapMarkerRowSelected"));
	if (const URokUiPageWidget* Page = RowClass && SelectedRowClass ? MountModePage() : nullptr)
	{
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const FString Prefix = FString::Printf(TEXT("MapMarkerPage_%d"), Index);
			const bool bSelected = Index == SelectedMapMarkerPage;
			Page->SetShown(Page->Get<UCanvasPanel>(*(Prefix + TEXT("Selected"))), bSelected);
			Page->SetShown(Page->Get<UCanvasPanel>(*(Prefix + TEXT("Normal"))), !bSelected);
			BindPageAction(*Page, *Prefix, 900 + Index);
		}
		int32 VisibleRow = 0;
		for (int32 Index = 0; Index < WorldMapPreviewMarkerCount; ++Index)
		{
			const FWorldMapPreviewMarker& Marker = WorldMapPreviewMarkers[Index];
			if (!DoesMarkerMatchPage(Marker, SelectedMapMarkerPage))
			{
				continue;
			}
			UCanvasPanel* RowSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("MapMarkerRowSlot_%d"), VisibleRow++));
			UClass* StyleClass = Index == SelectedMapMarkerIndex ? SelectedRowClass : RowClass;
			if (const URokUiPageWidget* Row = MountPageItem(RowSlot, StyleClass, TEXT("MapMarker"), Index))
			{
				if (UTextBlock* Name = Row->Get<UTextBlock>(TEXT("Name")))
				{
					Name->SetText(FText::FromString(Marker.Name));
				}
				if (UTextBlock* Description = Row->Get<UTextBlock>(TEXT("Description")))
				{
					Description->SetText(FormatMapMarkerDescription(Marker));
				}
				BindPageAction(*Row, TEXT("RowHit"), 920 + Index);
			}
		}
		Page->SetShown(Page->Get<UTextBlock>(TEXT("MapMarkerEmpty")), VisibleRow == 0);
		BindPageAction(*Page, TEXT("MapMarkerAdd"), 941);
		BindPageAction(*Page, TEXT("MapMarkerGo"), 940);
		return;
	}
	BuildSplitWindowFrame(
		CapturePoint(180.0f, 90.0f),
		FVector2D(1080.0f, 700.0f),
		NSLOCTEXT("RokMapMarker", "Title", "地图标记"),
		CapturePoint(1200.0f, 86.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("MapMarkerContentShade"),
			FLinearColor(0.035f, 0.04f, 0.04f, 0.98f)),
		CapturePoint(205.0f, 166.0f),
		FVector2D(1030.0f, 592.0f),
		2);

	static const FText PageLabels[] = {
		NSLOCTEXT("RokMapMarker", "All", "全部"),
		NSLOCTEXT("RokMapMarker", "Special", "特殊"),
		NSLOCTEXT("RokMapMarker", "Friends", "友方"),
		NSLOCTEXT("RokMapMarker", "Enemy", "敌方"),
		NSLOCTEXT("RokMapMarker", "Alliance", "联盟")};
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const bool bSelected = Index == SelectedMapMarkerPage;
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(TEXT("MapMarkerPage_%d"), Index),
				900 + Index,
				CommonAtlasPath,
				bSelected ? PageSelectedRect : PageNormalRect,
				CommonAtlasSize,
				FVector2D(184.0f, 45.0f),
				PageLabels[Index],
				20,
				bSelected
					? FLinearColor::White
					: FLinearColor(0.7f, 0.7f, 0.68f, 1.0f),
				ESlateBrushDrawType::Box,
				FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
			CapturePoint(244.0f + Index * 184.0f, 178.0f),
			FVector2D(184.0f, 45.0f),
			6);
	}

	int32 VisibleRow = 0;
	for (int32 Index = 0; Index < WorldMapPreviewMarkerCount; ++Index)
	{
		const FWorldMapPreviewMarker& Marker = WorldMapPreviewMarkers[Index];
		if (!DoesMarkerMatchPage(Marker, SelectedMapMarkerPage))
		{
			continue;
		}
		const bool bSelected = Index == SelectedMapMarkerIndex;
		const FVector2D RowPosition =
			CapturePoint(250.0f, 246.0f + VisibleRow * 92.0f);
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				*FString::Printf(TEXT("MapMarkerRowShade_%d"), Index),
				bSelected
					? FLinearColor(0.18f, 0.28f, 0.3f, 1.0f)
					: FLinearColor(0.055f, 0.065f, 0.065f, 1.0f)),
			RowPosition,
			FVector2D(900.0f, 78.0f),
			4);
		UTextBlock* Name = MakeScreenText(
			*FString::Printf(TEXT("MapMarkerName_%d"), Index),
			21,
			bSelected
				? FLinearColor(1.0f, 0.88f, 0.48f, 1.0f)
				: FLinearColor(0.9f, 0.9f, 0.88f, 1.0f),
			true);
		Name->SetText(FText::FromString(Marker.Name));
		PlaceOnCanvas(
			ModeCanvas,
			Name,
			RowPosition + FVector2D(22.0f, 8.0f),
			FVector2D(300.0f, 32.0f),
			5);
		UTextBlock* Description = MakeScreenText(
			*FString::Printf(TEXT("MapMarkerDescription_%d"), Index),
			16,
			FLinearColor(0.7f, 0.74f, 0.72f, 1.0f));
		Description->SetText(FormatMapMarkerDescription(Marker));
		PlaceOnCanvas(
			ModeCanvas,
			Description,
			RowPosition + FVector2D(22.0f, 42.0f),
			FVector2D(600.0f, 28.0f),
			5);
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(TEXT("MapMarkerRowHit_%d"), Index),
				920 + Index,
				nullptr,
				FIntRect(),
				FIntPoint(1, 1),
				FVector2D(900.0f, 78.0f)),
			RowPosition,
			FVector2D(900.0f, 78.0f),
			8);
		++VisibleRow;
	}

	if (VisibleRow == 0)
	{
		UTextBlock* Empty = MakeScreenText(
			TEXT("MapMarkerEmpty"),
			22,
			FLinearColor(0.7f, 0.7f, 0.68f, 1.0f),
			false,
			ETextJustify::Center);
		Empty->SetText(
			NSLOCTEXT("RokMapMarker", "Empty", "当前分类没有地图标记"));
		PlaceOnCanvas(
			ModeCanvas,
			Empty,
			CapturePoint(360.0f, 430.0f),
			FVector2D(720.0f, 50.0f),
			5);
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("MapMarkerAdd"),
			941,
			CommonAtlasPath,
			MediumGoldButtonRect,
			CommonAtlasSize,
			FVector2D(202.0f, 70.0f),
			NSLOCTEXT("RokMapMarker", "Add", "添加标记"),
			22,
			FLinearColor(0.25f, 0.13f, 0.02f, 1.0f),
			ESlateBrushDrawType::Box,
			FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		CapturePoint(506.0f, 666.0f),
		FVector2D(202.0f, 70.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("MapMarkerGo"),
			940,
			CommonAtlasPath,
			MediumBlueButtonRect,
			CommonAtlasSize,
			FVector2D(202.0f, 70.0f),
			NSLOCTEXT("RokMapMarker", "Go", "前往"),
			22,
			FLinearColor(0.02f, 0.17f, 0.29f, 1.0f),
			ESlateBrushDrawType::Box,
			FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		CapturePoint(724.0f, 666.0f),
		FVector2D(202.0f, 70.0f),
		7);
}

void URokUnityFeatureScreenWidget::BuildResourceSearch()
{
	static const FText TypeLabels[] = {
		NSLOCTEXT("RokResourceSearch", "Barbarian", "乱军"),
		NSLOCTEXT("RokResourceSearch", "Food", "农田"),
		NSLOCTEXT("RokResourceSearch", "Wood", "伐木场"),
		NSLOCTEXT("RokResourceSearch", "Stone", "铁矿床"),
		NSLOCTEXT("RokResourceSearch", "Gold", "金矿床")};
	static const FText TypeDescriptions[] = {
		NSLOCTEXT("RokResourceSearch", "BarbarianTip", "击败乱军能够获得丰厚的奖励"),
		NSLOCTEXT("RokResourceSearch", "FoodTip", "派遣攻城车能够采集更多的食物。"),
		NSLOCTEXT("RokResourceSearch", "WoodTip", "派遣攻城车能够采集更多的木材。"),
		NSLOCTEXT("RokResourceSearch", "StoneTip", "派遣攻城车能够采集更多的铁矿石。"),
		NSLOCTEXT("RokResourceSearch", "GoldTip", "派遣攻城车能够采集更多的金币。")};
	UTexture2D* SearchAtlas = LoadUiTexture(ResourceSearchAtlasPath);
	UMaterialInstanceDynamic* LockedIcon = nullptr;
	if (auto* Gray = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/RokPrototype/UI/Research/M_RokResearchGrayUI.M_RokResearchGrayUI")))
	{
		LockedIcon = UMaterialInstanceDynamic::Create(Gray, this);
		LockedIcon->SetTextureParameterValue(TEXT("SourceAtlas"), SearchAtlas);
	}
	SelectedResourceSearchType = FMath::Clamp(SelectedResourceSearchType, 0, 4);
	const int32 SelectedLevel = SelectedResourceSearchType == 0
		? ResourceSearchBarbarianLevel
		: ResourceSearchResourceLevel;
	const int32 MaxLevel = SelectedResourceSearchType == 0 ? 25 : 6;
	const bool bShowTaskGuide = bResourceSearchTaskGuide && SelectedResourceSearchType == 0;
	if (bShowTaskGuide && !TaskGuideTexture)
	{
		TaskGuideTexture = FImageUtils::ImportFileAsTexture2D(RokDataPaths::Resolve(TEXT("WorldSearch/finger001.png")));
	}

	UClass* TypeClass = ResolveFeaturePageClass(TEXT("ResourceSearchType"));
	if (const URokUiPageWidget* Page = TypeClass ? MountModePage(NAME_None, false) : nullptr)
	{
		BindPageAction(*Page, TEXT("ResourceSearchTouchCloseMask"), 1013);
		BindPageAction(*Page, TEXT("ResourceSearchLevelMinus"), 1010);
		BindPageAction(*Page, TEXT("ResourceSearchLevelPlus"), 1011);
		BindPageAction(*Page, TEXT("ResourceSearchSubmit"), 1012);
		ResourceSearchDockRoot = Page->Get<UCanvasPanel>(TEXT("ResourceSearchDockRoot"));
		ResourceSearchPopupRoot = Page->Get<UCanvasPanel>(TEXT("ResourceSearchPopupRoot"));
		const auto TypeSlotName = [](int32 Index)
		{
			return FName(*FString::Printf(TEXT("ResourceSearchTypeSlot_%d"), Index));
		};
		const auto TypeSlotX = [Page, &TypeSlotName](int32 Index)
		{
			const UCanvasPanel* TypeSlot = Page->Get<UCanvasPanel>(TypeSlotName(Index));
			const UCanvasPanelSlot* Layout = TypeSlot ? Cast<UCanvasPanelSlot>(TypeSlot->Slot) : nullptr;
			return Layout ? Layout->GetPosition().X : 0.0;
		};
		if (UCanvasPanelSlot* PopupSlot = ResourceSearchPopupRoot ? Cast<UCanvasPanelSlot>(ResourceSearchPopupRoot->Slot) : nullptr)
		{
			PopupSlot->SetPosition(PopupSlot->GetPosition()
				+ FVector2D(TypeSlotX(SelectedResourceSearchType) - TypeSlotX(0), 0.0));
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(TypeLabels); ++Index)
		{
			UCanvasPanel* TypeSlot = Page->Get<UCanvasPanel>(TypeSlotName(Index));
			if (const URokUiPageWidget* TypePage = MountPageItem(TypeSlot, TypeClass, TEXT("ResourceSearch"), Index))
			{
				BindPageAction(*TypePage, TEXT("Type"), 1000 + Index);
				if (UTextBlock* Label = TypePage->Get<UTextBlock>(TEXT("TypeLabel")))
				{
					Label->SetText(TypeLabels[Index]);
				}
				if (UImage* Icon = TypePage->Get<UImage>(TEXT("TypeIcon")))
				{
					FSlateBrush Brush = Icon->GetBrush();
					const auto AtlasBrush = MakeAtlasBrush(SearchAtlas, ResourceSearchTypeIconRects[Index],
						ResourceSearchAtlasSize, Brush.GetImageSize());
					const bool bLocked = !GetResourceSearchLock(Index).IsEmpty();
					Brush.SetResourceObject(bLocked && LockedIcon ? static_cast<UObject*>(LockedIcon) : SearchAtlas);
					Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
					Icon->SetBrush(Brush);
				}
			}
		}
		if (UTextBlock* Description = Page->Get<UTextBlock>(TEXT("ResourceSearchDescription")))
		{
			Description->SetText(TypeDescriptions[SelectedResourceSearchType]);
		}
		ResourceSearchLevelValueText = Page->Get<UTextBlock>(TEXT("ResourceSearchLevelValue"));
		ResourceSearchSliderFill = Page->Get<UProgressBar>(TEXT("ResourceSearchSliderFill"));
		ResourceSearchMonsterTip = Page->Get<UTextBlock>(TEXT("ResourceSearchMonsterTip"));
		ResourceSearchSlider = Page->Get<USlider>(TEXT("ResourceSearchSlider"));
		if (ResourceSearchSlider)
		{
			ResourceSearchSlider->SetMinValue(1.0f);
			ResourceSearchSlider->SetMaxValue(static_cast<float>(MaxLevel));
			ResourceSearchSlider->SetValue(static_cast<float>(SelectedLevel));
			ResourceSearchSlider->OnValueChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleResourceSearchSliderChanged);
		}
		HandleResourceSearchSliderChanged(static_cast<float>(SelectedLevel));
		UImage* Guide = Page->Get<UImage>(TEXT("ResourceSearchGuide"));
		const bool bShowGuide = bShowTaskGuide && TaskGuideTexture;
		Page->SetPresent(Guide, bShowGuide);
		ResourceSearchGuide = bShowGuide ? Guide : nullptr;
		if (ResourceSearchGuide)
		{
			ResourceSearchGuide->SetBrushFromTexture(TaskGuideTexture);
		}
		return;
	}

	// Unity registers this interface as kTouchCloseAlpha: the map remains
	// visible and a transparent full-screen raycast mask closes the popover.
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("ResourceSearchTouchCloseMask"),
			1013,
			nullptr,
			FIntRect(),
			FIntPoint::ZeroValue,
			FVector2D(UnityReferenceWidth, UnityReferenceHeight)),
		FVector2D::ZeroVector,
		FVector2D(UnityReferenceWidth, UnityReferenceHeight),
		0);

	const FVector2D DockPosition(163.0f, 625.0f);
	const FVector2D DockSize(1088.0f, 170.0f);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(
			TEXT("ResourceSearchDockBackground"),
			CommonAtlasPath,
			CommonPanelRect,
			CommonAtlasSize,
			DockSize,
			CommonPanelMargin),
		DockPosition,
		DockSize,
		2);

	for (int32 TypeIndex = 0; TypeIndex < UE_ARRAY_COUNT(TypeLabels); ++TypeIndex)
	{
		const FVector2D CellPosition(200.0f + TypeIndex * 220.0f, 637.0f);
		const FVector2D CellSize(137.0f, 151.0f);
		UCanvasPanel* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("ResourceSearchTypeCell_%d"), TypeIndex));
		PlaceOnCanvas(
			Cell,
			MakeAtlasImage(
				*FString::Printf(TEXT("ResourceSearchTypeFrame_%d"), TypeIndex),
				ResourceSearchAtlasPath,
				ResourceSearchTypeFrameRect,
				ResourceSearchAtlasSize,
				CellSize),
			FVector2D::ZeroVector,
			CellSize,
			0);
		const bool bLocked = !GetResourceSearchLock(TypeIndex).IsEmpty();
		auto* Icon = MakeAtlasImage(*FString::Printf(TEXT("ResourceSearchTypeIcon_%d"), TypeIndex),
			ResourceSearchAtlasPath, ResourceSearchTypeIconRects[TypeIndex], ResourceSearchAtlasSize,
			FVector2D(144.0f, 160.0f));
		if (bLocked && LockedIcon)
		{
			FSlateBrush Brush = Icon->GetBrush();
			Brush.SetResourceObject(LockedIcon);
			Icon->SetBrush(Brush);
		}
		PlaceOnCanvas(Cell, Icon, FVector2D(-3.5f, -2.3f), FVector2D(144.0f, 160.0f), 1);
		UTextBlock* TypeLabel = MakeScreenText(
			*FString::Printf(TEXT("ResourceSearchTypeLabel_%d"), TypeIndex),
			18,
			FLinearColor::White,
			true,
			ETextJustify::Center);
		TypeLabel->SetText(TypeLabels[TypeIndex]);
		TypeLabel->SetFont(URokMainHudWidget::MakeUnitySourceFont(22, false));
		PlaceOnCanvas(
			Cell,
			TypeLabel,
			FVector2D(8.5f, 104.2f),
			FVector2D(120.0f, 30.0f),
			2);
		PlaceOnCanvas(ModeCanvas, Cell, CellPosition, CellSize, 3);
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(TEXT("ResourceSearchType_%d"), TypeIndex),
				1000 + TypeIndex,
				nullptr,
				FIntRect(),
				FIntPoint::ZeroValue,
				CellSize),
			CellPosition,
			CellSize,
			10);
	}

	const float PopupCenterX = 280.0f + SelectedResourceSearchType * 220.0f;
	const FVector2D PopupPosition(PopupCenterX - 200.0f, 334.8f);
	const FVector2D PopupSize(400.0f, 307.0f);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(
			TEXT("ResourceSearchPopup"),
			CommonAtlasPath,
			ResourceSearchPopupRect,
			CommonAtlasSize,
			PopupSize,
			ResourceSearchPopupMargin),
		PopupPosition,
		PopupSize,
		5);

	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(
			TEXT("ResourceSearchDescriptionPanel"),
			CommonAtlasPath,
			CommonPanelRect,
			CommonAtlasSize,
			FVector2D(360.0f, 78.31f),
			CommonPanelMargin),
		FVector2D(PopupCenterX - 180.0f, 357.545f),
		FVector2D(360.0f, 78.31f),
		6);
	UTextBlock* Description = MakeScreenText(
		TEXT("ResourceSearchDescription"),
		18,
		FLinearColor(0.89f, 0.86f, 0.78f, 1.0f),
		false,
		ETextJustify::Center);
	const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(48, 46, 39));
	Description->SetText(TypeDescriptions[SelectedResourceSearchType]);
	Description->SetFont(URokMainHudWidget::MakeUnitySourceFont(20, true));
	Description->SetColorAndOpacity(Ink);
	Description->SetShadowColorAndOpacity(FLinearColor::Transparent);
	Description->SetAutoWrapText(true);
	PlaceOnCanvas(
		ModeCanvas,
		Description,
		FVector2D(PopupCenterX - 170.0f, 384.0f),
		FVector2D(340.0f, 56.0f),
		8);

	ResourceSearchLevelValueText = MakeScreenText(
		TEXT("ResourceSearchLevelValue"),
		20,
		FLinearColor(0.96f, 0.91f, 0.78f, 1.0f),
		true,
		ETextJustify::Left);
	ResourceSearchLevelValueText->SetFont(URokMainHudWidget::MakeUnitySourceFont(21, true));
	ResourceSearchLevelValueText->SetColorAndOpacity(Ink);
	ResourceSearchLevelValueText->SetShadowColorAndOpacity(FLinearColor::Transparent);
	PlaceOnCanvas(
		ModeCanvas,
		ResourceSearchLevelValueText,
		FVector2D(PopupCenterX - 130.0f, 440.0f),
		FVector2D(193.24f, 30.0f),
		8);

	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(
			TEXT("ResourceSearchSliderBackground"),
			CommonAtlasPath,
			ResourceSearchSliderBackgroundRect,
			CommonAtlasSize,
			FVector2D(230.0f, 23.0f),
			ResourceSearchSliderBackgroundMargin),
		FVector2D(PopupCenterX - 115.0f, 475.0f),
		FVector2D(230.0f, 23.0f),
		7);
	ResourceSearchSliderFill = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(),
		TEXT("ResourceSearchSliderFill"));
	FSlateBrush EmptyBrush;
	EmptyBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	FProgressBarStyle ProgressStyle;
	ProgressStyle.SetBackgroundImage(EmptyBrush);
	ProgressStyle.SetFillImage(MakeAtlasBrush(
		LoadUiTexture(CommonAtlasPath),
		ResourceSearchSliderFillRect,
		CommonAtlasSize,
		FVector2D(12.0f, 19.0f)));
	ProgressStyle.SetMarqueeImage(EmptyBrush);
	ResourceSearchSliderFill->SetWidgetStyle(ProgressStyle);
	ResourceSearchSliderFill->SetBarFillType(EProgressBarFillType::LeftToRight);
	PlaceOnCanvas(
		ModeCanvas,
		ResourceSearchSliderFill,
		FVector2D(PopupCenterX - 115.0f, 477.0f),
		FVector2D(230.0f, 19.0f),
		8);

	ResourceSearchSlider = WidgetTree->ConstructWidget<USlider>(
		USlider::StaticClass(),
		TEXT("ResourceSearchSlider"));
	ResourceSearchSlider->SetMinValue(1.0f);
	ResourceSearchSlider->SetMaxValue(static_cast<float>(MaxLevel));
	ResourceSearchSlider->SetStepSize(1.0f);
	ResourceSearchSlider->MouseUsesStep = true;
	ResourceSearchSlider->SetIndentHandle(false);
	ResourceSearchSlider->SetValue(static_cast<float>(SelectedLevel));
	FSliderStyle SliderStyle;
	SliderStyle.SetNormalBarImage(EmptyBrush);
	SliderStyle.SetHoveredBarImage(EmptyBrush);
	SliderStyle.SetDisabledBarImage(EmptyBrush);
	const FSlateBrush ThumbBrush = MakeAtlasBrush(
		LoadUiTexture(CommonAtlasPath),
		ResourceSearchSliderThumbRect,
		CommonAtlasSize,
		FVector2D(20.0f, 40.0f));
	SliderStyle.SetNormalThumbImage(ThumbBrush);
	SliderStyle.SetHoveredThumbImage(ThumbBrush);
	SliderStyle.SetDisabledThumbImage(ThumbBrush);
	SliderStyle.SetBarThickness(19.0f);
	ResourceSearchSlider->SetWidgetStyle(SliderStyle);

	ResourceSearchSlider->OnValueChanged.AddDynamic(
		this,
		&URokUnityFeatureScreenWidget::HandleResourceSearchSliderChanged);
	PlaceOnCanvas(
		ModeCanvas,
		ResourceSearchSlider,
		FVector2D(PopupCenterX - 115.0f, 466.5f),
		FVector2D(230.0f, 40.0f),
		11);

	const FVector2D MinusPosition(PopupCenterX - 188.05f, 454.2f);
	const FVector2D PlusPosition(PopupCenterX + 127.63f, 454.0f);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(
			TEXT("ResourceSearchLevelPlusIcon"),
			CommonAtlasPath,
			ItemPlusRect,
			CommonAtlasSize,
			FVector2D(50.0f)),
		PlusPosition + FVector2D(7.0f),
		FVector2D(50.0f),
		8);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("ResourceSearchLevelPlus"),
			1011,
			nullptr,
			FIntRect(),
			FIntPoint::ZeroValue,
			FVector2D(64.0f)),
		PlusPosition,
		FVector2D(64.0f),
		12);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(
			TEXT("ResourceSearchLevelMinusIcon"),
			CommonAtlasPath,
			ItemMinusRect,
			CommonAtlasSize,
			FVector2D(50.0f)),
		MinusPosition + FVector2D(7.0f),
		FVector2D(50.0f),
		8);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("ResourceSearchLevelMinus"),
			1010,
			nullptr,
			FIntRect(),
			FIntPoint::ZeroValue,
			FVector2D(64.0f)),
		MinusPosition,
		FVector2D(64.0f),
		12);

	ResourceSearchMonsterTip = MakeScreenText(TEXT("ResourceSearchMonsterTip"), 16, Ink, false, ETextJustify::Center);
	ResourceSearchMonsterTip->SetFont(URokMainHudWidget::MakeUnitySourceFont(20, false));
	ResourceSearchMonsterTip->SetShadowColorAndOpacity(FLinearColor::Transparent);
	PlaceOnCanvas(ModeCanvas, ResourceSearchMonsterTip, FVector2D(PopupCenterX - 175, 513), FVector2D(350, 26), 9);
	HandleResourceSearchSliderChanged(static_cast<float>(SelectedLevel));

	auto* Submit = MakeActionButton(
			TEXT("ResourceSearchSubmit"),
			1012,
			CommonAtlasPath,
			ResourceSearchSubmitRect,
			CommonAtlasSize,
			FVector2D(192.0f, 78.0f),
			NSLOCTEXT("RokResourceSearch", "Search", "搜索"),
			25,
			Ink);
	PlaceOnCanvas(ModeCanvas, Submit, FVector2D(PopupCenterX - 96.0f, 543.8f), FVector2D(192.0f, 78.0f), 12);
	if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("ResourceSearchSubmitLabel"))))
	{
		Label->SetFont(URokMainHudWidget::MakeUnitySourceFont(32, true));
		Label->SetShadowColorAndOpacity(FLinearColor::Transparent);
	}
	ResourceSearchGuide = nullptr;
	if (bShowTaskGuide && TaskGuideTexture)
	{
		ResourceSearchGuide = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ResourceSearchGuide"));
		ResourceSearchGuide->SetBrushFromTexture(TaskGuideTexture);
		ResourceSearchGuide->SetRenderTransformAngle(180.0f);
		ResourceSearchGuide->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(ModeCanvas, ResourceSearchGuide, FVector2D(PopupCenterX - 24.5f, 383), FVector2D(49, 149), 14);
	}

	UImage* Arrow = MakeAtlasImage(
		TEXT("ResourceSearchPopupArrow"),
		CommonAtlasPath,
		ResourceSearchPopupArrowRect,
		CommonAtlasSize,
		FVector2D(12.0f, 15.0f));
	Arrow->SetRenderTransformPivot(FVector2D(0.5f));
	Arrow->SetRenderTransformAngle(-90.0f);
	PlaceOnCanvas(
		ModeCanvas,
		Arrow,
		FVector2D(PopupCenterX - 6.0f, 637.6f),
		FVector2D(12.0f, 15.0f),
		9);

	// Recreate the two source animation groups without moving settled content.
	// pl_Pop is 100x100, centered at bottom+75, not at the popup body's center.
	const TArray<UWidget*> SearchChildren = ModeCanvas->GetAllChildren();
	ResourceSearchDockRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResourceSearchDockRoot"));
	ResourceSearchPopupRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ResourceSearchPopupRoot"));
	ResourceSearchDockRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ResourceSearchPopupRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ResourceSearchPopupRoot->SetRenderTransformPivot(FVector2D(0.5f));
	// The source dock settles against the expanded viewport's bottom edge.
	constexpr float BottomOffset = 44.0f;
	const FVector2D PopupRootPosition(PopupCenterX - 50, UnityReferenceHeight - 125 + BottomOffset);
	PlaceOnCanvas(ModeCanvas, ResourceSearchDockRoot, FVector2D(0, BottomOffset), FVector2D(UnityReferenceWidth, UnityReferenceHeight), 2);
	PlaceOnCanvas(ModeCanvas, ResourceSearchPopupRoot, PopupRootPosition, FVector2D(100), 5);
	for (UWidget* Child : SearchChildren)
	{
		const FString Name = Child->GetName();
		if (Name == TEXT("ResourceSearchTouchCloseMask")) continue;
		const bool bDock = Name.StartsWith(TEXT("ResourceSearchType")) || Name == TEXT("ResourceSearchDockBackground");
		const auto* OldSlot = CastChecked<UCanvasPanelSlot>(Child->Slot);
		const FVector2D Position = OldSlot->GetPosition();
		const FVector2D Size = OldSlot->GetSize();
		const int32 ZOrder = OldSlot->GetZOrder();
		Child->RemoveFromParent();
		PlaceOnCanvas(bDock ? ResourceSearchDockRoot.Get() : ResourceSearchPopupRoot.Get(), Child,
			Position + (bDock ? FVector2D::ZeroVector : FVector2D(0, BottomOffset) - PopupRootPosition), Size, ZOrder);
	}
}

void URokUnityFeatureScreenWidget::AdvanceExpeditionAnimation(float DeltaSeconds)
{
	if (ExpeditionAnimationPhase.IsNone() || ExpeditionAnimationPhase == TEXT("Idle")) return;
	const bool bToPreview = ExpeditionAnimationPhase == TEXT("PreviewTransition");
	const float Duration = bToPreview ? 0.33333334f : 0.5833333f;
	ExpeditionAnimationElapsed = FMath::Clamp(
		ExpeditionAnimationElapsed + FMath::Max(0.0f, DeltaSeconds), 0.0f, Duration);
	const float Time = ExpeditionAnimationElapsed;
	const auto Segment = [](float A, float B, float At, float From, float To)
	{
		const float U = FMath::Clamp((At - A) / (B - A), 0.0f, 1.0f);
		return FMath::Lerp(From, To, U * U * (3.0f - 2.0f * U));
	};
	// UA_MainIF_Right_On/Off: zero-tangent Hermite, 0.25 seconds.
	const float SideX = bToPreview
		? (Time <= .1f ? Segment(0, .1f, Time, 0, -20)
			: Segment(.1f, .25f, Time, -20, 100))
		: (Time <= .15f ? Segment(0, .15f, Time, 100, -20)
			: Segment(.15f, .25f, Time, -20, 0));
	const float SideAlpha = bToPreview
		? Segment(0, .25f, Time, 1, 0)
		: Segment(0, .25f, Time, 0, 1);
	if (UCanvasPanel* Side = ExpeditionSideRoot.Get())
	{
		Side->SetVisibility(Time >= Duration && bToPreview
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		Side->SetRenderTranslation(FVector2D(SideX, 0));
		Side->SetRenderOpacity(SideAlpha);
	}
	if (UCanvasPanel* Preview = ExpeditionPreviewRoot.Get())
	{
		const float PreviewX = bToPreview
			? (Time <= .15f ? Segment(0, .15f, Time, 100, -20)
				: Segment(.15f, .25f, Time, -20, 0))
			: (Time <= .1f ? Segment(0, .1f, Time, 0, -20)
				: Segment(.1f, .25f, Time, -20, 100));
		Preview->SetVisibility(Time >= Duration && !bToPreview
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		Preview->SetRenderTranslation(FVector2D(PreviewX, 0));
		Preview->SetRenderOpacity(bToPreview
			? Segment(0, .25f, Time, 0, 1)
			: Segment(0, .25f, Time, 1, 0));
	}
	if (UCanvasPanel* Center = ExpeditionCenterRoot.Get())
	{
		// CenterHide animates the root. CenterShow animates the nested img and
		// pl_flagMes, not this root (UA_MainIF_Center Show/Hide clips). Native
		// CenterHide keeps the artwork visible but both root raycast booleans
		// false throughout the clip; CenterShow keeps them true.
		Center->SetVisibility(bToPreview
			? (Time >= Duration ? ESlateVisibility::Collapsed
				: ESlateVisibility::HitTestInvisible)
			: ESlateVisibility::SelfHitTestInvisible);
		Center->SetRenderScale(FVector2D(bToPreview
			? Segment(0, .33333334f, Time, 1, 1.05f) : 1.0f));
		Center->SetRenderOpacity(bToPreview
			? Segment(0, .33333334f, Time, 1, 0) : 1.0f);
	}
	if (UCanvasPanel* Img = ExpeditionCenterImg.Get())
	{
		// The native CenterShow img localScale keys are 1.05, .9, 1, 1.
		const float Scale = bToPreview ? 1.0f
			: (Time <= .16666667f ? Segment(0, .16666667f, Time, 1.05f, .9f)
				: Time <= .33333334f ? Segment(.16666667f, .33333334f, Time, .9f, 1.0f)
				: 1.0f);
		Img->SetRenderScale(FVector2D(Scale));
	}
	if (UCanvasPanel* FlagMes = ExpeditionCenterFlagMes.Get())
	{
		// Native pl_flagMes: alpha 0 through .3333 then 1 at .5833;
		// anchoredPosition.y 10 through .3333 then 0 at .5833.
		FlagMes->SetRenderOpacity(bToPreview ? 1.0f
			: (Time <= .33333334f ? 0.0f
				: Segment(.33333334f, .5833333f, Time, 0.0f, 1.0f)));
		FlagMes->SetRenderTranslation(FVector2D(0.0f, bToPreview ? 0.0f
			: (Time <= .33333334f ? -10.0f
				: -Segment(.33333334f, .5833333f, Time, 10.0f, 0.0f))));
	}
	if (UWidget* PreviewButton = GetWidgetFromName(TEXT("ExpeditionPreviewButton")))
	{
		PreviewButton->SetVisibility(bExpeditionPreview
			? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (UWidget* TaskChallenge = GetWidgetFromName(TEXT("ExpeditionChallenge")))
	{
		// The map button is rebuilt on another branch. Only this Task button
		// follows the CenterHide/Show input state.
		TaskChallenge->SetIsEnabled(!bToPreview && !ExpeditionTeams.IsEmpty());
	}
	if (Time >= Duration) ExpeditionAnimationPhase = FName(TEXT("Idle"));
}

namespace
{
FText FormatGemPackageAmount(const FRokUnityEconomyChargeGemPackage& Package)
{
	return FText::Format(NSLOCTEXT("RokGemShop", "GemAmount", "{0} 宝石"), FText::AsNumber(Package.DenarNum));
}

FText FormatGemPackageBonus(const FRokUnityEconomyChargeGemPackage& Package)
{
	return FText::Format(NSLOCTEXT("RokGemShop", "BonusConfigFormat", "首充赠 {0}\n常规赠 {1}"),
		FText::AsNumber(Package.FirstPresenter), FText::AsNumber(Package.Presenter));
}

FText FormatGemPackageProduct(const FRokUnityEconomyChargeGemPackage& Package)
{
	return FText::Format(NSLOCTEXT("RokGemShop", "ProductConfigFormat", "商品配置 {0}（不是现金价格）"),
		FText::AsNumber(Package.ProductId));
}
}

void URokUnityFeatureScreenWidget::BuildGemShop()
{
	// Packages come from the Unity RechargeGemMall rows in the economy
	// catalog; ProductId is a config key, not a cash price, so the shop keeps
	// the same honest product note as the charge screen.
	URokUnityEconomyCatalogSubsystem* EconomyCatalog = GetEconomyCatalogSubsystem();
	if (EconomyCatalog && !EconomyCatalog->IsCatalogLoaded())
	{
		EconomyCatalog->LoadCatalog();
	}
	const TArray<FRokUnityEconomyChargeGemPackage> GemPackages =
		EconomyCatalog && EconomyCatalog->IsCatalogLoaded()
			? EconomyCatalog->GetChargeGemPackages()
			: TArray<FRokUnityEconomyChargeGemPackage>();
	UClass* PackageClass = ResolveFeaturePageClass(TEXT("GemShopPackage"));
	if (const URokUiPageWidget* Page = PackageClass ? MountModePage() : nullptr)
	{
		const int32 PackageCount = FMath::Min(6, GemPackages.Num());
		for (int32 Index = 0; Index < PackageCount; ++Index)
		{
			UCanvasPanel* CardSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("GemShopPackageSlot_%d"), Index));
			const URokUiPageWidget* Card = MountPageItem(CardSlot, PackageClass, TEXT("GemShop"), Index);
			if (!Card)
			{
				continue;
			}
			const FRokUnityEconomyChargeGemPackage& Package = GemPackages[Index];
			if (UTextBlock* Amount = Card->Get<UTextBlock>(TEXT("PackageAmount")))
			{
				Amount->SetText(FormatGemPackageAmount(Package));
			}
			if (UTextBlock* Bonus = Card->Get<UTextBlock>(TEXT("PackageBonus")))
			{
				Bonus->SetText(FormatGemPackageBonus(Package));
			}
			if (UTextBlock* Product = Card->Get<UTextBlock>(TEXT("PackageProduct")))
			{
				Product->SetText(FormatGemPackageProduct(Package));
			}
			BindPageAction(*Card, TEXT("Buy"), 1200 + Index);
		}
		Page->SetShown(Page->Get<UTextBlock>(TEXT("GemShopCatalogMissing")), PackageCount == 0);
		UE_LOG(LogTemp, Display, TEXT("Rok gem shop screen: packages=%d balance=not_synced authority=server."), PackageCount);
		return;
	}
	BuildSplitWindowFrame(
		CapturePoint(155.0f, 62.0f),
		FVector2D(1130.0f, 726.0f),
		NSLOCTEXT("RokGemShop", "Title", "宝石商店"),
		CapturePoint(1225.0f, 58.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("GemShopContentShade"),
			FLinearColor(0.055f, 0.035f, 0.055f, 0.99f)),
		CapturePoint(181.0f, 140.0f),
		FVector2D(1078.0f, 612.0f),
		2);

	UTextBlock* Balance = MakeScreenText(
		TEXT("GemShopBalance"),
		22,
		FLinearColor(0.96f, 0.8f, 0.86f, 1.0f),
		true,
		ETextJustify::Right);
	// Gem balance is server currency state; without a role snapshot the shop
	// must not present a fabricated amount.
	Balance->SetText(
		NSLOCTEXT("RokGemShop", "BalanceUnavailable", "当前宝石：未同步"));
	PlaceOnCanvas(
		ModeCanvas,
		Balance,
		CapturePoint(900.0f, 158.0f),
		FVector2D(300.0f, 40.0f),
		5);

	int32 ShownPackages = 0;
	for (int32 Index = 0; Index < FMath::Min(6, GemPackages.Num()); ++Index)
	{
		const FRokUnityEconomyChargeGemPackage& Package = GemPackages[Index];
		const int32 Column = Index % 3;
		const int32 Row = Index / 3;
		const float X = 320.0f + Column * 300.0f;
		const float Y = 248.0f + Row * 226.0f;
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				*FString::Printf(TEXT("GemShopPackageShade_%d"), Index),
				FLinearColor(0.12f, 0.075f, 0.13f, 1.0f)),
			CapturePoint(X, Y),
			FVector2D(274.0f, 202.0f),
			3);
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				*FString::Printf(TEXT("GemShopPackageIcon_%d"), Index),
				Item1AtlasPath,
				SpriteRect(1, 513, 126, 126),
				Item1AtlasSize,
				FVector2D(78.0f)),
			CapturePoint(X + 12.0f, Y + 20.0f),
			FVector2D(78.0f),
			5);
		UTextBlock* Amount = MakeScreenText(
			*FString::Printf(TEXT("GemShopPackageAmount_%d"), Index),
			23,
			FLinearColor(1.0f, 0.78f, 0.88f, 1.0f),
			true);
		Amount->SetText(FormatGemPackageAmount(Package));
		PlaceOnCanvas(
			ModeCanvas,
			Amount,
			CapturePoint(X + 102.0f, Y + 18.0f),
			FVector2D(156.0f, 38.0f),
			5);
		UTextBlock* Bonus = MakeScreenText(
			*FString::Printf(TEXT("GemShopPackageBonus_%d"), Index),
			14,
			FLinearColor(0.93f, 0.8f, 0.4f, 1.0f));
		Bonus->SetText(FormatGemPackageBonus(Package));
		PlaceOnCanvas(
			ModeCanvas,
			Bonus,
			CapturePoint(X + 102.0f, Y + 58.0f),
			FVector2D(162.0f, 50.0f),
			5);
		UTextBlock* Product = MakeScreenText(
			*FString::Printf(TEXT("GemShopPackageProduct_%d"), Index),
			12,
			FLinearColor(0.68f, 0.67f, 0.68f, 1.0f));
		Product->SetText(FormatGemPackageProduct(Package));
		PlaceOnCanvas(
			ModeCanvas,
			Product,
			CapturePoint(X + 16.0f, Y + 116.0f),
			FVector2D(244.0f, 24.0f),
			5);
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(TEXT("GemShopBuy_%d"), Index),
				1200 + Index,
				CommonAtlasPath,
				MediumGoldButtonRect,
				CommonAtlasSize,
				FVector2D(184.0f, 56.0f),
				NSLOCTEXT("RokGemShop", "BuyAction", "购买"),
				20,
				FLinearColor(0.25f, 0.13f, 0.02f, 1.0f),
				ESlateBrushDrawType::Box,
				FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
			CapturePoint(X + 45.0f, Y + 146.0f),
			FVector2D(184.0f, 56.0f),
			7);
		++ShownPackages;
	}
	if (ShownPackages == 0)
	{
		UTextBlock* CatalogMissing = MakeScreenText(
			TEXT("GemShopCatalogMissing"),
			18,
			FLinearColor(0.72f, 0.72f, 0.7f, 1.0f),
			false,
			ETextJustify::Center);
		CatalogMissing->SetText(NSLOCTEXT(
			"RokGemShop",
			"CatalogMissing",
			"宝石礼包配置未加载，无法展示商品。"));
		PlaceOnCanvas(
			ModeCanvas,
			CatalogMissing,
			CapturePoint(360.0f, 400.0f),
			FVector2D(720.0f, 40.0f),
			5);
	}

	UTextBlock* BoundaryNote = MakeScreenText(
		TEXT("GemShopBoundaryNote"),
		15,
		FLinearColor(0.65f, 0.62f, 0.65f, 1.0f),
		false,
		ETextJustify::Center);
	BoundaryNote->SetText(
		NSLOCTEXT(
			"RokGemShop",
			"BoundaryNote",
			"支付与宝石到账必须由服务器确认，请在联网后购买"));
	PlaceOnCanvas(
		ModeCanvas,
		BoundaryNote,
		CapturePoint(360.0f, 680.0f),
		FVector2D(720.0f, 30.0f),
		5);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok gem shop screen: packages=%d balance=not_synced authority=server."),
		ShownPackages);
}

namespace
{
FText FormatChargeCatalogState(const FRokUnityEconomyChargePage* Page)
{
	return Page
		? FText::Format(
			NSLOCTEXT(
				"RokCharge",
				"CatalogSourceFormat",
				"Unity RechargeList · 行 {0} · ID {1} · 类型 {2}"),
			FText::AsNumber(Page->SourceRow),
			FText::AsNumber(Page->Id),
			FText::AsNumber(Page->PagingType))
		: NSLOCTEXT(
			"RokCharge",
			"CatalogUnavailable",
			"Unity 充值目录尚未加载；当前仅保留页面结构。");
}

FText FormatChargeGemAmount(const FRokUnityEconomyChargeGemPackage& Package)
{
	return FText::Format(
		NSLOCTEXT("RokCharge", "GemAmount", "{0} 宝石"),
		FText::AsNumber(Package.DenarNum));
}

FText FormatChargeGemBonus(const FRokUnityEconomyChargeGemPackage& Package)
{
	return FText::Format(
		NSLOCTEXT(
			"RokCharge",
			"BonusConfigFormat",
			"首充赠 {0}\n常规赠 {1}"),
		FText::AsNumber(Package.FirstPresenter),
		FText::AsNumber(Package.Presenter));
}

FText FormatChargeGemProduct(const FRokUnityEconomyChargeGemPackage& Package)
{
	return FText::Format(
		NSLOCTEXT(
			"RokCharge",
			"ProductConfigFormat",
			"商品配置 {0}（不是现金价格）"),
		FText::AsNumber(Package.ProductId));
}

FText FormatChargeServiceState(int32 PageType)
{
	return FText::Format(
		NSLOCTEXT(
			"RokCharge",
			"PageBoundaryFormat",
			"{0} 的页面入口与 Unity 配置已接入。\n活动进度、限购、领奖资格和商品内容仍需服务器状态。"),
		ResolveChargePageLabel(PageType));
}

void SetFeaturePageText(const URokUiPageWidget& Page, FName Name, const FText& Value)
{
	if (UTextBlock* Text = Page.Get<UTextBlock>(Name))
	{
		Text->SetText(Value);
	}
}

FSlateColor FeaturePresentationColor(const URokUiPageWidget* Presentation, FName Name, const FLinearColor& Fallback)
{
	const auto* State = Presentation ? Presentation->Get<UTextBlock>(Name) : nullptr;
	return State ? State->GetColorAndOpacity() : FSlateColor(Fallback);
}
}

void URokUnityFeatureScreenWidget::BuildCharge()
{
	URokUnityEconomyCatalogSubsystem* Catalog =
		GetEconomyCatalogSubsystem();
	if (Catalog && !Catalog->IsCatalogLoaded())
	{
		Catalog->LoadCatalog();
	}
	const bool bCatalogLoaded = Catalog && Catalog->IsCatalogLoaded();
	static const int32 ChargePageOrder[] = {
		101,
		102,
		103,
		104,
		107,
		105,
		106};
	if (!FMath::IsWithinInclusive(SelectedChargePageType, 101, 107))
	{
		SelectedChargePageType = 107;
	}
	FRokUnityEconomyChargePage SelectedPage;
	const bool bHasPageConfig = Catalog
		&& Catalog->FindChargePageByPagingType(
			SelectedChargePageType,
			SelectedPage);
	UClass* CardClass = ResolveFeaturePageClass(TEXT("ChargeGemPackage"));
	if (const URokUiPageWidget* Page = CardClass ? MountModePage() : nullptr)
	{
		for (int32 PageType : ChargePageOrder)
		{
			const FString Prefix = FString::Printf(TEXT("ChargePage_%d"), PageType);
			const bool bSelected = PageType == SelectedChargePageType;
			Page->SetShown(Page->Get<UCanvasPanel>(*(Prefix + TEXT("Selected"))), bSelected);
			Page->SetShown(Page->Get<UCanvasPanel>(*(Prefix + TEXT("Normal"))), !bSelected);
			BindPageAction(*Page, *Prefix, 2300 + PageType - 101);
		}
		SetFeaturePageText(*Page, TEXT("ChargeSelectedPageTitle"), ResolveChargePageLabel(SelectedChargePageType));
		SetFeaturePageText(*Page, TEXT("ChargeCatalogState"), FormatChargeCatalogState(bHasPageConfig ? &SelectedPage : nullptr));

		const bool bGemPage = SelectedChargePageType == 107;
		for (const FName ServiceWidget : {FName(TEXT("ChargePageServicePanel")),
			FName(TEXT("ChargeSelectedPageServiceBoundary")), FName(TEXT("ChargePageBoundaryAction"))})
		{
			Page->SetPresent(Page->Get<UWidget>(ServiceWidget), !bGemPage);
		}
		if (!bGemPage)
		{
			Page->SetPresent(Page->Get<UTextBlock>(TEXT("ChargeGemCatalogEmptyState")), false);
			SetFeaturePageText(*Page, TEXT("ChargeSelectedPageServiceBoundary"), FormatChargeServiceState(SelectedChargePageType));
			BindPageAction(*Page, TEXT("ChargePageBoundaryAction"), 2340);
			return;
		}

		const TArray<FRokUnityEconomyChargeGemPackage> Packages = bCatalogLoaded
			? Catalog->GetChargeGemPackages() : TArray<FRokUnityEconomyChargeGemPackage>();
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("ChargeGemCatalogEmptyState")), Packages.IsEmpty());
		for (int32 Index = 0; Index < FMath::Min(6, Packages.Num()); ++Index)
		{
			UCanvasPanel* CardSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("ChargeGemPackageSlot_%d"), Index));
			if (const URokUiPageWidget* Card = MountPageItem(CardSlot, CardClass, TEXT("ChargeGemPackage"), Index))
			{
				const FRokUnityEconomyChargeGemPackage& Package = Packages[Index];
				SetFeaturePageText(*Card, TEXT("Amount"), FormatChargeGemAmount(Package));
				SetFeaturePageText(*Card, TEXT("Bonus"), FormatChargeGemBonus(Package));
				SetFeaturePageText(*Card, TEXT("Product"), FormatChargeGemProduct(Package));
				BindPageAction(*Card, TEXT("Buy"), 2320 + Index);
			}
		}
		return;
	}

	BuildSplitWindowFrame(
		CapturePoint(80.0f, 45.0f),
		FVector2D(1280.0f, 780.0f),
		NSLOCTEXT("RokCharge", "Title", "充值"),
		CapturePoint(1298.0f, 48.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("ChargeContentShade"),
			FLinearColor(0.045f, 0.04f, 0.045f, 0.99f)),
		CapturePoint(94.0f, 125.0f),
		FVector2D(1252.0f, 670.0f),
		2);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("ChargePageListShade"),
			FLinearColor(0.10f, 0.09f, 0.105f, 0.98f)),
		CapturePoint(110.0f, 145.0f),
		FVector2D(232.0f, 624.0f),
		3);

	for (int32 Index = 0;
		Index < UE_ARRAY_COUNT(ChargePageOrder);
		++Index)
	{
		const int32 PageType = ChargePageOrder[Index];
		const bool bSelected = PageType == SelectedChargePageType;
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(TEXT("ChargePage_%d"), PageType),
				2300 + PageType - 101,
				CommonAtlasPath,
				bSelected ? PageSelectedRect : PageNormalRect,
				CommonAtlasSize,
				FVector2D(210.0f, 62.0f),
				ResolveChargePageLabel(PageType),
				21,
				bSelected
					? FLinearColor::White
					: FLinearColor(0.72f, 0.71f, 0.69f, 1.0f),
				ESlateBrushDrawType::Box,
				FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
			CapturePoint(121.0f, 166.0f + Index * 80.0f),
			FVector2D(210.0f, 62.0f),
			6);
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("ChargePageContentShade"),
			FLinearColor(0.12f, 0.09f, 0.12f, 0.98f)),
		CapturePoint(360.0f, 145.0f),
		FVector2D(965.0f, 624.0f),
		3);
	UTextBlock* PageTitle = MakeScreenText(
		TEXT("ChargeSelectedPageTitle"),
		30,
		FLinearColor(0.98f, 0.86f, 0.54f, 1.0f),
		true);
	PageTitle->SetText(ResolveChargePageLabel(SelectedChargePageType));
	PlaceOnCanvas(
		ModeCanvas,
		PageTitle,
		CapturePoint(390.0f, 163.0f),
		FVector2D(430.0f, 48.0f),
		6);
	UTextBlock* Balance = MakeScreenText(
		TEXT("ChargeGemBalanceState"),
		18,
		FLinearColor(0.92f, 0.82f, 0.88f, 1.0f),
		false,
		ETextJustify::Right);
	Balance->SetText(NSLOCTEXT(
		"RokCharge",
		"GemBalanceBoundary",
		"宝石余额：等待服务器同步"));
	PlaceOnCanvas(
		ModeCanvas,
		Balance,
		CapturePoint(930.0f, 170.0f),
		FVector2D(360.0f, 36.0f),
		6);

	UTextBlock* SourceState = MakeScreenText(
		TEXT("ChargeCatalogState"),
		15,
		FLinearColor(0.67f, 0.65f, 0.67f, 1.0f));
	SourceState->SetText(FormatChargeCatalogState(bHasPageConfig ? &SelectedPage : nullptr));
	PlaceOnCanvas(
		ModeCanvas,
		SourceState,
		CapturePoint(390.0f, 207.0f),
		FVector2D(670.0f, 28.0f),
		6);

	if (SelectedChargePageType == 107)
	{
		const TArray<FRokUnityEconomyChargeGemPackage> Packages =
			bCatalogLoaded
				? Catalog->GetChargeGemPackages()
				: TArray<FRokUnityEconomyChargeGemPackage>();
		for (int32 Index = 0;
			Index < FMath::Min(6, Packages.Num());
			++Index)
		{
			const FRokUnityEconomyChargeGemPackage& Package =
				Packages[Index];
			const int32 Column = Index % 3;
			const int32 Row = Index / 3;
			const FVector2D CardPosition(
				390.0f + Column * 300.0f,
				248.0f + Row * 226.0f);
			PlaceOnCanvas(
				ModeCanvas,
				MakeSolidBorder(
					*FString::Printf(
						TEXT("ChargeGemPackageShade_%d"),
						Index),
					FLinearColor(0.19f, 0.12f, 0.19f, 1.0f)),
				CapturePoint(CardPosition.X, CardPosition.Y),
				FVector2D(274.0f, 202.0f),
				4);
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					*FString::Printf(
						TEXT("ChargeGemPackageIcon_%d"),
						Index),
					Item1AtlasPath,
					SpriteRect(1, 513, 126, 126),
					Item1AtlasSize,
					FVector2D(78.0f)),
				CapturePoint(
					CardPosition.X + 12.0f,
					CardPosition.Y + 20.0f),
				FVector2D(78.0f),
				6);
			UTextBlock* Amount = MakeScreenText(
				*FString::Printf(
					TEXT("ChargeGemPackageAmount_%d"),
					Index),
				23,
				FLinearColor(1.0f, 0.78f, 0.90f, 1.0f),
				true);
			Amount->SetText(FormatChargeGemAmount(Package));
			PlaceOnCanvas(
				ModeCanvas,
				Amount,
				CapturePoint(
					CardPosition.X + 102.0f,
					CardPosition.Y + 18.0f),
				FVector2D(156.0f, 38.0f),
				6);
			UTextBlock* Bonus = MakeScreenText(
				*FString::Printf(
					TEXT("ChargeGemPackageBonus_%d"),
					Index),
				14,
				FLinearColor(0.92f, 0.81f, 0.45f, 1.0f));
			Bonus->SetText(FormatChargeGemBonus(Package));
			PlaceOnCanvas(
				ModeCanvas,
				Bonus,
				CapturePoint(
					CardPosition.X + 102.0f,
					CardPosition.Y + 58.0f),
				FVector2D(162.0f, 50.0f),
				6);
			UTextBlock* Product = MakeScreenText(
				*FString::Printf(
					TEXT("ChargeGemPackageProduct_%d"),
					Index),
				13,
				FLinearColor(0.68f, 0.67f, 0.68f, 1.0f));
			Product->SetText(FormatChargeGemProduct(Package));
			PlaceOnCanvas(
				ModeCanvas,
				Product,
				CapturePoint(
					CardPosition.X + 16.0f,
					CardPosition.Y + 112.0f),
				FVector2D(244.0f, 27.0f),
				6);
			PlaceOnCanvas(
				ModeCanvas,
				MakeActionButton(
					*FString::Printf(
						TEXT("ChargeGemPackageBuy_%d"),
						Index),
					2320 + Index,
					CommonAtlasPath,
					MediumGoldButtonRect,
					CommonAtlasSize,
					FVector2D(180.0f, 52.0f),
					NSLOCTEXT("RokCharge", "Buy", "购买"),
					20,
					FLinearColor(0.25f, 0.13f, 0.02f, 1.0f),
					ESlateBrushDrawType::Box,
					FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
				CapturePoint(
					CardPosition.X + 47.0f,
					CardPosition.Y + 143.0f),
				FVector2D(180.0f, 52.0f),
				7);
		}

		if (Packages.IsEmpty())
		{
			UTextBlock* Empty = MakeScreenText(
				TEXT("ChargeGemCatalogEmptyState"),
				22,
				FLinearColor(0.83f, 0.80f, 0.76f, 1.0f),
				true,
				ETextJustify::Center);
			Empty->SetText(NSLOCTEXT(
				"RokCharge",
				"GemCatalogEmpty",
				"RechargeGemMall 目录尚未加载，未显示虚构礼包。"));
			PlaceOnCanvas(
				ModeCanvas,
				Empty,
				CapturePoint(510.0f, 420.0f),
				FVector2D(650.0f, 48.0f),
				6);
		}
	}
	else
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				TEXT("ChargePageServicePanel"),
				FLinearColor(0.17f, 0.14f, 0.14f, 0.98f)),
			CapturePoint(420.0f, 270.0f),
			FVector2D(840.0f, 360.0f),
			4);
		UTextBlock* PageBoundary = MakeScreenText(
			TEXT("ChargeSelectedPageServiceBoundary"),
			23,
			FLinearColor(0.94f, 0.90f, 0.80f, 1.0f),
			true,
			ETextJustify::Center);
		PageBoundary->SetText(FormatChargeServiceState(SelectedChargePageType));
		PageBoundary->SetAutoWrapText(true);
		PlaceOnCanvas(
			ModeCanvas,
			PageBoundary,
			CapturePoint(475.0f, 340.0f),
			FVector2D(730.0f, 120.0f),
			6);
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				TEXT("ChargePageBoundaryAction"),
				2340,
				CommonAtlasPath,
				MediumBlueButtonRect,
				CommonAtlasSize,
				FVector2D(260.0f, 66.0f),
				NSLOCTEXT("RokCharge", "ServerPending", "等待服务器同步"),
				20,
				FLinearColor(0.04f, 0.14f, 0.24f, 1.0f),
				ESlateBrushDrawType::Box,
				MiniBlueMargin),
			CapturePoint(710.0f, 500.0f),
			FVector2D(260.0f, 66.0f),
			7);
	}

	UTextBlock* ServiceBoundary = MakeScreenText(
		TEXT("ChargeServiceBoundaryState"),
		15,
		FLinearColor(0.75f, 0.72f, 0.74f, 1.0f),
		false,
		ETextJustify::Center);
	ServiceBoundary->SetText(NSLOCTEXT(
		"RokCharge",
		"ServiceBoundary",
		"本地只展示 Unity 配置；余额、资格、本地化价格、下单、校验与到账必须由服务器和支付 SDK 确认。"));
	PlaceOnCanvas(
		ModeCanvas,
		ServiceBoundary,
		CapturePoint(390.0f, 732.0f),
		FVector2D(900.0f, 32.0f),
		8);
}

#include "RokFeatureBuildCity.inl"

namespace
{
FRokServerBuildQueueSnapshot ReadBuildingSpeedUpSnapshot(const URokServerCitySubsystem* City)
{
	if (GetBuildingSpeedUpPreview() == INDEX_NONE)
	{
		return City ? City->GetBuildQueueSnapshot() : FRokServerBuildQueueSnapshot();
	}
	FRokServerBuildQueueSnapshot Snapshot;
	Snapshot.bAvailable = true;
	Snapshot.bTimeSynchronized = true;
	auto& Queue = Snapshot.Queues.AddDefaulted_GetRef();
	Queue.QueueIndex = 1;
	Queue.State = ERokServerBuildQueueState::Busy;
	Queue.BuildingIndex = 5;
	Queue.BuildingName = TEXT("市政厅");
	Queue.TargetLevel = 3;
	Queue.bCountdownKnown = Queue.bProgressKnown = true;
	Queue.RemainingSeconds = 90;
	Queue.TotalSeconds = 180;
	Queue.Progress = 0.5f;
	return Snapshot;
}

FRokServerBuildQueueSnapshot ReadWorkerQueuePresentation(const URokServerCitySubsystem* City)
{
	const int32 Preview = ReadLocalUiPreviewOption(TEXT("RokWorkerQueueFixture="), 1, 3);
	if (Preview == INDEX_NONE)
	{
		return ReadBuildingSpeedUpSnapshot(City);
	}
	FRokServerBuildQueueSnapshot Snapshot;
	Snapshot.bAvailable = true;
	Snapshot.bTimeSynchronized = true;
	Snapshot.Queues.SetNum(2);
	FRokServerBuildQueueView& Primary = Snapshot.Queues[0];
	FRokServerBuildQueueView& Secondary = Snapshot.Queues[1];
	Primary.QueueIndex = 1;
	Secondary.QueueIndex = 2;
	Primary.State = ERokServerBuildQueueState::Idle;
	Secondary.State = Preview == 1 ? ERokServerBuildQueueState::Locked : ERokServerBuildQueueState::Idle;
	if (Preview == 2)
	{
		Primary.State = Secondary.State = ERokServerBuildQueueState::Busy;
		Primary.BuildingName = TEXT("市政厅");
		Secondary.BuildingName = TEXT("农场");
		Primary.TargetLevel = 3;
		Secondary.TargetLevel = 2;
		Primary.bCountdownKnown = Primary.bProgressKnown = true;
		Secondary.bCountdownKnown = Secondary.bProgressKnown = true;
		Primary.Progress = 0.5f;
		Secondary.Progress = 0.25f;
		Primary.RemainingSeconds = 90;
		Secondary.RemainingSeconds = 45;
	}
	else if (Preview == 3)
	{
		Secondary.LeaseRemainingSeconds = 5;
	}
	return Snapshot;
}

ERokServerBuildQueueState ResolveWorkerQueueState(
	const FRokServerBuildQueueView* Queue)
{
	return Queue ? Queue->State : ERokServerBuildQueueState::Unknown;
}

FText FormatWorkerCountdown(double Remaining)
{
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(Remaining));
	const FString Clock = FString::Printf(TEXT("%02d:%02d:%02d"), (Seconds / 3600) % 24, (Seconds / 60) % 60, Seconds % 60);
	return Seconds < 86400 ? FText::FromString(Clock)
		: FText::Format(NSLOCTEXT("RokWorkerQueue", "CountdownDays", "{0}天 {1}"),
			FText::AsNumber(Seconds / 86400), FText::FromString(Clock));
}
}

void URokUnityFeatureScreenWidget::BuildWorkerQueue()
{
	const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	const auto Queues = ReadWorkerQueuePresentation(City);
	const auto* Secondary = Queues.FindQueue(2);
	const auto* Upgrades = GetWorld() ? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
	bWorkerQueueRenderedServer = Queues.bAvailable;
	bWorkerQueueRenderedBusy = !Queues.bAvailable && Upgrades && Upgrades->GetFirstActiveUpgradeActor();
	const auto PrimaryState = Queues.bAvailable ? ResolveWorkerQueueState(Queues.FindQueue(1))
		: (bWorkerQueueRenderedBusy ? ERokServerBuildQueueState::Busy : ERokServerBuildQueueState::Idle);
	const auto SecondaryState = ResolveWorkerQueueState(Secondary);
	const bool bRenew = Secondary && SecondaryState == ERokServerBuildQueueState::Idle
		&& Queues.bTimeSynchronized && Secondary->LeaseRemainingSeconds >= 0 && Secondary->LeaseRemainingSeconds < WorkerRequiredSeconds;
	bWorkerQueueRenderedRenew = bRenew;
	const bool bHire = SecondaryState == ERokServerBuildQueueState::Locked;
	WorkerQueueRenderedPrimaryState = int32(PrimaryState);
	WorkerQueueRenderedSecondaryState = int32(SecondaryState);

	const auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	FString Failure;
	bWorkerQueueCanWrite = Net && Net->CanRequestCityWrite(Failure);
	int64 Items = 0, Gems = 0;
	const bool bItemsKnown = Net && Net->TryGetOwnedItemCount(URokGameNetSubsystem::WorkerHireItemId, Items);
	bWorkerQueueRenderedItemsKnown = bItemsKnown;
	bWorkerHireUsesItem = bItemsKnown && Items > 0;
	WorkerHirePrice = bWorkerHireUsesItem ? 1 : URokGameNetSubsystem::WorkerHireGemCost;
	bWorkerHireAffordable = bItemsKnown && (bWorkerHireUsesItem || (RokSprotoJson::TryGetInt64(
		Net->GetRoleInfo(), TEXT("denar"), Gems) && Gems >= WorkerHirePrice));

	// UI_Win_Worker (1120 x 700) and its two UI_Item_Worker rows (1024 x 137).
	const FText Title = bRenew ? NSLOCTEXT("RokWorkerQueue", "ContractExpires", "您的额外建造队列很快就要到期啦")
		: WorkerRequiredSeconds > 0 ? NSLOCTEXT("RokWorkerQueue", "WorkersBusy", "建筑工人正忙着呢！")
		: NSLOCTEXT("RokWorkerQueue", "ManageWorkers", "建筑工人管理");
	const FText Currency = bWorkerHireUsesItem ? NSLOCTEXT("RokWorkerQueue", "HireItem", "建筑工人雇佣券")
		: NSLOCTEXT("RokWorkerQueue", "HireGems", "宝石");
	FText PlacementNote;
	if (WorkerRequiredSeconds > 0)
	{
		if (bRenew)
		{
			PlacementNote = FText::Format(NSLOCTEXT("RokWorkerQueue", "RenewForPlacement",
				"城主，额外建筑工人的雇佣合同将在{0}后到期，而新的建造队列需要{1}来完成。您可以选择使用{2}来延长雇佣合同，或者使用道具来加速正在进行中的建造队列。"),
				FormatWorkerCountdown(Secondary->LeaseRemainingSeconds), FormatWorkerCountdown(WorkerRequiredSeconds), Currency);
		}
		else if (bHire)
		{
			PlacementNote = FText::Format(NSLOCTEXT("RokWorkerQueue", "HireForPlacement",
				"城主，您的建筑工人正在忙碌中。您可以选择使用{0}来雇佣额外的建筑工人，或者使用道具来加速正在进行中的建造队列。"), Currency);
		}
		else if (SecondaryState == ERokServerBuildQueueState::Busy)
		{
			PlacementNote = NSLOCTEXT("RokWorkerQueue", "SpeedForPlacement",
				"城主，您的建筑工人正在忙碌中。您可以选择加速完成进行中的建造队列，再开始新的建造。");
		}
	}
	const TCHAR* HireIconPath = CommonAtlasPath;
	FIntRect HireIconRect = GemRect;
	FIntPoint HireIconSize = CommonAtlasSize;
	FRokUnityEconomyItem HireItem;
	if ((bHire || bRenew) && bWorkerHireUsesItem && GetEconomyCatalogSubsystem()
		&& GetEconomyCatalogSubsystem()->FindItemById(URokGameNetSubsystem::WorkerHireItemId, HireItem))
	{
		ResolveEconomyItemIcon(HireItem, HireIconPath, HireIconRect, HireIconSize);
	}
	const FText PriceText = bWorkerHireUsesItem ? FText::FromString(TEXT("x 1")) : FText::AsNumber(WorkerHirePrice);
	const FLinearColor PriceColor = bWorkerHireAffordable ? FLinearColor::White : FLinearColor(1, .32f, .26f);
	const auto ActionText = [](bool bContract, bool bBusy)
	{
		return bContract ? NSLOCTEXT("RokWorkerQueue", "Hire", "雇佣")
			: bBusy ? NSLOCTEXT("RokWorkerQueue", "SpeedUp", "加速") : NSLOCTEXT("RokWorkerQueue", "StartBuild", "建造");
	};
	const auto IllustrationRect = [](ERokServerBuildQueueState State)
	{
		return State == ERokServerBuildQueueState::Busy ? WorkerBusyRect
			: State == ERokServerBuildQueueState::Idle ? WorkerIdleRect : WorkerLockedRect;
	};

	UClass* PrimaryClass = ResolveFeaturePageClass(TEXT("WorkerQueuePrimaryRow"));
	UClass* SecondaryClass = ResolveFeaturePageClass(TEXT("WorkerQueueSecondaryRow"));
	if (const URokUiPageWidget* Page = PrimaryClass && SecondaryClass ? MountModePage() : nullptr)
	{
		SetFeaturePageText(*Page, TEXT("FeatureFrameTitle"), Title);
		UTextBlock* Note = Page->Get<UTextBlock>(TEXT("WorkerQueuePlacementNote"));
		Page->SetPresent(Note, WorkerRequiredSeconds > 0);
		if (Note)
		{
			Note->SetText(PlacementNote);
		}
		auto ApplyAtlas = [this](UImage* Image, const TCHAR* Path, const FIntRect& Rect, const FIntPoint& Size)
		{
			if (Image)
			{
				UTexture2D* Atlas = LoadUiTexture(Path);
				if (!Atlas)
				{
					Image->SetVisibility(ESlateVisibility::Collapsed);
					return;
				}
				FSlateBrush Brush = Image->GetBrush();
				const FSlateBrush AtlasBrush = MakeAtlasBrush(Atlas, Rect, Size, Brush.GetImageSize());
				Brush.SetResourceObject(Atlas);
				Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
				Image->SetBrush(Brush);
			}
		};
		for (int32 Index : {1, 2})
		{
			const bool bSecond = Index == 2;
			const auto State = bSecond ? SecondaryState : PrimaryState;
			const bool bBusy = State == ERokServerBuildQueueState::Busy;
			const bool bContract = bSecond && (bHire || bRenew);
			const auto Name = [bSecond](const TCHAR* First, const TCHAR* Second) { return bSecond ? Second : First; };
			const URokUiPageWidget* Row = MountPageItem(
				Page->Get<UCanvasPanel>(Name(TEXT("WorkerQueuePrimaryRowSlot"), TEXT("WorkerQueueSecondaryRowSlot"))),
				bSecond ? SecondaryClass : PrimaryClass, TEXT(""), Index);
			if (!Row)
			{
				continue;
			}

			ApplyAtlas(Row->Get<UImage>(Name(TEXT("WorkerQueuePrimaryIllustration"), TEXT("WorkerQueueSecondaryIllustration"))),
				WorkerMatrixAtlasPath, IllustrationRect(State), WorkerMatrixAtlasSize);
			auto* RowTitle = Row->Get<UTextBlock>(Name(TEXT("WorkerQueuePrimaryTitle"), TEXT("WorkerQueueSecondaryTitle")));
			auto* RowNote = Row->Get<UTextBlock>(Name(TEXT("WorkerQueueRuntimeState"), TEXT("WorkerQueueSecondaryNote")));
			auto* Progress = Row->Get<UProgressBar>(Name(TEXT("WorkerQueuePrimaryProgress"), TEXT("WorkerQueueSecondaryProgress")));
			auto* Time = Row->Get<UTextBlock>(Name(TEXT("WorkerQueuePrimaryProgressText"), TEXT("WorkerQueueSecondaryProgressText")));
			Row->SetShown(Time, bBusy);
			if (bSecond)
			{
				WorkerQueueSecondaryTitleText = RowTitle;
				WorkerQueueSecondaryNoteText = RowNote;
				WorkerQueueSecondaryProgress = Progress;
				WorkerQueueSecondaryProgressText = Time;
			}
			else
			{
				WorkerQueuePrimaryTitleText = RowTitle;
				WorkerQueueRuntimeStateText = RowNote;
				WorkerQueuePrimaryProgress = Progress;
				WorkerQueuePrimaryProgressText = Time;
			}

			const FName ActionName = Name(TEXT("WorkerQueuePrimaryAction"), TEXT("WorkerQueueSecondQueueUnlockOrExtend"));
			BindPageAction(*Row, ActionName, bSecond ? 1420 : 1400);
			if (UButton* Button = Row->Get<UButton>(ActionName))
			{
				Button->SetIsEnabled(bContract ? bWorkerQueueCanWrite && bItemsKnown : State != ERokServerBuildQueueState::Unknown);
			}
			const bool bTwoLines = bContract || bBusy;
			Row->SetPresent(Row->Get<UTextBlock>(Name(TEXT("WorkerQueuePrimaryActionLabel"), TEXT("WorkerQueueSecondQueueUnlockOrExtendLabel"))), !bTwoLines);
			UTextBlock* ButtonLabel = Row->Get<UTextBlock>(Name(TEXT("WorkerQueuePrimaryButtonLabel"), TEXT("WorkerQueueSecondaryButtonLabel")));
			Row->SetPresent(ButtonLabel, bTwoLines);
			if (ButtonLabel)
			{
				ButtonLabel->SetText(ActionText(bContract, bBusy));
			}
			if (bSecond)
			{
				Row->SetPresent(Row->Get<UCanvasPanel>(TEXT("WorkerQueueSecondQueueUnlockOrExtendBackground")), !bContract);
				Row->SetPresent(Row->Get<UCanvasPanel>(TEXT("WorkerQueueHireBackground")), bContract);
				UImage* Icon = Row->Get<UImage>(TEXT("WorkerQueueHireCurrency"));
				UTextBlock* Price = Row->Get<UTextBlock>(TEXT("WorkerQueueHireCost"));
				Row->SetPresent(Icon, bContract && HireIconPath);
				Row->SetPresent(Price, bContract);
				Row->SetPresent(Row->Get<UTextBlock>(TEXT("WorkerQueueHireTerm")), bContract);
				if (bContract)
				{
					ApplyAtlas(Icon, HireIconPath, HireIconRect, HireIconSize);
					if (Price)
					{
						Price->SetText(PriceText);
						FTextBlockStyle PriceStyle;
						const bool bAuthoredPrice = TryGetFeatureTextStyle(bWorkerHireAffordable
							? TEXT("WorkerQueueHirePriceAffordable") : TEXT("WorkerQueueHirePriceInsufficient"), PriceStyle);
						Price->SetColorAndOpacity(bAuthoredPrice ? PriceStyle.ColorAndOpacity : FSlateColor(PriceColor));
					}
				}
			}
		}
		UpdateWorkerQueuePresentation();
		return;
	}

	BuildSplitWindowFrame(CapturePoint(160, 100), FVector2D(1120, 700), Title, CapturePoint(1210, 92), nullptr, 1, 0, 30);
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("WorkerQueueContentShade"), FLinearColor(.045f, .047f, .043f, .99f)),
		CapturePoint(208, 162), FVector2D(1024, 612), 2);

	for (int32 Index : {1, 2})
	{
		const bool bSecond = Index == 2;
		const auto State = bSecond ? SecondaryState : PrimaryState;
		const bool bBusy = State == ERokServerBuildQueueState::Busy;
		const bool bContract = bSecond && (bHire || bRenew);
		const float Y = bSecond ? 323.5f : 186.5f;
		const auto Name = [bSecond](const TCHAR* First, const TCHAR* Second) { return bSecond ? Second : First; };
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(Name(TEXT("WorkerQueuePrimaryIllustration"), TEXT("WorkerQueueSecondaryIllustration")),
			WorkerMatrixAtlasPath, IllustrationRect(State),
			WorkerMatrixAtlasSize, FVector2D(179, 137)), CapturePoint(1029.5f, Y), FVector2D(179, 137), 4);
		auto* RowTitle = MakeScreenText(Name(TEXT("WorkerQueuePrimaryTitle"), TEXT("WorkerQueueSecondaryTitle")), 26,
			FLinearColor(.886f, .82f, .608f), false);
		RowTitle->SetFont(URokMainHudWidget::MakeUnitySourceFont(24, false));
		RowTitle->SetAutoWrapText(true);
		PlaceOnCanvas(ModeCanvas, RowTitle, CapturePoint(460, Y + 9), FVector2D(542, 60), 5);
		auto* Note = MakeScreenText(Name(TEXT("WorkerQueueRuntimeState"), TEXT("WorkerQueueSecondaryNote")), 19, FLinearColor::White);
		Note->SetFont(URokMainHudWidget::MakeUnitySourceFont(20, false));
		PlaceOnCanvas(ModeCanvas, Note, CapturePoint(460, Y + 46), FVector2D(542, 38.7f), 5);
		auto* Progress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),
			Name(TEXT("WorkerQueuePrimaryProgress"), TEXT("WorkerQueueSecondaryProgress")));
		FProgressBarStyle ProgressStyle;
		auto Track = MakeAtlasBrush(LoadUiTexture(CommonAtlasPath), SpriteRect(401, 112, 20, 23), CommonAtlasSize, FVector2D(20, 23));
		Track.DrawAs = ESlateBrushDrawType::Box;
		Track.Margin = FMargin(.5f, 7.f/23, .5f, 6.f/23);
		ProgressStyle.SetBackgroundImage(Track).SetFillImage(MakeAtlasBrush(LoadUiTexture(CommonAtlasPath),
			VipProgressFillRect, CommonAtlasSize, FVector2D(12, 19)));
		Progress->SetWidgetStyle(ProgressStyle);
		PlaceOnCanvas(ModeCanvas, Progress, CapturePoint(460, Y + 90), FVector2D(546, 24), 5);
		auto* Time = MakeScreenText(Name(TEXT("WorkerQueuePrimaryProgressText"), TEXT("WorkerQueueSecondaryProgressText")),
			17, FLinearColor::White, true, ETextJustify::Center);
		if (bSecond)
		{
			WorkerQueueSecondaryTitleText = RowTitle;
			WorkerQueueSecondaryNoteText = Note;
			WorkerQueueSecondaryProgress = Progress;
			WorkerQueueSecondaryProgressText = Time;
		}
		else
		{
			WorkerQueuePrimaryTitleText = RowTitle;
			WorkerQueueRuntimeStateText = Note;
			WorkerQueuePrimaryProgress = Progress;
			WorkerQueuePrimaryProgressText = Time;
		}
		const FVector2D ButtonPosition = CapturePoint(238, Y + 32);
		const bool bTwoLines = bContract || bBusy;
		const FText Label = ActionText(bContract, bBusy);
		auto* Button = MakeActionButton(Name(TEXT("WorkerQueuePrimaryAction"), TEXT("WorkerQueueSecondQueueUnlockOrExtend")),
			bSecond ? 1420 : 1400, CommonAtlasPath, bContract ? MediumGoldButtonRect : MediumBlueButtonRect,
			CommonAtlasSize, FVector2D(202, 84), bTwoLines ? FText::GetEmpty() : Label, 24,
			FLinearColor(.06f, .12f, .17f), ESlateBrushDrawType::Box, FMargin(.42f, 0, .42f, 0));
		Button->SetIsEnabled(bContract ? bWorkerQueueCanWrite && bItemsKnown : State != ERokServerBuildQueueState::Unknown);
		PlaceOnCanvas(ModeCanvas, Button, ButtonPosition, FVector2D(202, 84), 6);
		if (bTwoLines)
		{
			auto* ButtonLabel = MakeScreenText(Name(TEXT("WorkerQueuePrimaryButtonLabel"), TEXT("WorkerQueueSecondaryButtonLabel")),
				24, FLinearColor(.06f, .12f, .17f), true, ETextJustify::Center);
			ButtonLabel->SetText(Label);
			PlaceOnCanvas(ModeCanvas, ButtonLabel, ButtonPosition + FVector2D(0, 8), FVector2D(202, 34), 7);
		}
		if (bBusy) PlaceOnCanvas(ModeCanvas, Time, ButtonPosition + FVector2D(0, 47), FVector2D(202, 28), 7);
		if (bContract)
		{
			if (HireIconPath) PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("WorkerQueueHireCurrency"), HireIconPath, HireIconRect,
				HireIconSize, FVector2D(32)), ButtonPosition + FVector2D(58, 45), FVector2D(32), 7);
			auto* Price = MakeScreenText(TEXT("WorkerQueueHireCost"), 20, PriceColor, true);
			Price->SetText(PriceText);
			PlaceOnCanvas(ModeCanvas, Price, ButtonPosition + FVector2D(95, 48), FVector2D(94, 29), 7);
			auto* Term = MakeScreenText(TEXT("WorkerQueueHireTerm"), 18, FLinearColor::White, false, ETextJustify::Center);
			Term->SetText(NSLOCTEXT("RokWorkerQueue", "TwoDayContract", "签订2天合同"));
			PlaceOnCanvas(ModeCanvas, Term, ButtonPosition - FVector2D(0, 30), FVector2D(202, 30), 7);
		}
	}
	if (WorkerRequiredSeconds > 0)
	{
		auto* Note = MakeScreenText(TEXT("WorkerQueuePlacementNote"), 20, FLinearColor(.93f, .90f, .78f));
		Note->SetAutoWrapText(true);
		Note->SetText(PlacementNote);
		PlaceOnCanvas(ModeCanvas, Note, CapturePoint(243.6f, 482), FVector2D(952.8f, 180), 5);
	}
	UpdateWorkerQueuePresentation();
}

void URokUnityFeatureScreenWidget::BuildHospital()
{
	auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	const auto Role = ReadHospitalDisplayRole(Net, GetHospitalPreviewState());
	const auto Previous = HospitalState;
	HospitalState = FRokHospitalState::Read(Role);
	const bool bQueue = HospitalState.FinishTime > 0;
	const auto& Units = bQueue ? HospitalState.InTreatment : HospitalState.Wounded;
	if (Previous.FinishTime != HospitalState.FinishTime || !Previous.bKnown)
	{
		HospitalCounts.Reset();
	}
	for (auto It = HospitalCounts.CreateIterator(); It; ++It)
	{
		if (!Units.ContainsByPredicate([Id = It.Key()](const FRokHospitalSoldier& Unit) { return Unit.Id == Id; }))
			It.RemoveCurrent();
	}
	HospitalQuantities.Reset();
	HospitalConfirmation = nullptr;

	int64 Country = 0;
	int32 HospitalMark = INDEX_NONE;
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	// HospitalMediator.LoadFinish uses Civilization.hospitalMark to index RS.HospitalMarkFrame.
	if (!Config || !RokSprotoJson::TryGetInt64(Role, TEXT("country"), Country) || Country <= 0 || Country > MAX_int32
		|| !Config->GetIntFieldByKey(TEXT("Civilization"), TEXT("ID"), int32(Country), TEXT("hospitalMark"), HospitalMark)
		|| (HospitalMark != 0 && HospitalMark != 1))
	{
		HospitalMark = INDEX_NONE;
	}
	if (!BuildHospitalPage(Units, bQueue, HospitalMark))
	{
		BuildLegacyHospitalPage(Units, bQueue, HospitalMark);
	}

	HospitalExplanationWidget = WidgetTree->ConstructWidget<URokHospitalExplanationWidget>(
		URokHospitalExplanationWidget::ResolvePresentationClass(), TEXT("HospitalExplanationWidget"));
	if (HospitalExplanationWidget->InitializeExplanation())
	{
		PlaceOnCanvas(ModeCanvas, HospitalExplanationWidget, HospitalExplanationWidget->GetSourcePosition(),
			HospitalExplanationWidget->GetSourceSize(), 9);
		HospitalExplanationWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Hospital explanation: %s"), *HospitalExplanationWidget->GetLoadError());
		HospitalExplanationWidget = nullptr;
	}
	SetHospitalExplanationVisible(bHospitalExplanationVisible);
	UpdateHospitalActions();
}

bool URokUnityFeatureScreenWidget::BuildHospitalPage(const TArray<FRokHospitalSoldier>& Units,
	bool bQueue, int32 HospitalMark)
{
	UClass* RowClass = ResolveFeaturePageClass(TEXT("HospitalTroopRow"));
	auto* Page = RowClass ? MountModePage(TEXT("Hospital")) : nullptr;
	if (!Page)
	{
		return false;
	}
	auto* List = Page->Get<UScrollBox>(TEXT("HospitalTreatmentList"));
	HospitalMainContent = Page->Get<UCanvasPanel>(TEXT("HospitalMainContent"));
	if (!HospitalMainContent || !List)
	{
		Page->RemoveFromParent();
		ActiveModePage = nullptr;
		return false;
	}

	const bool bFull = !Units.IsEmpty();
	for (const FName Name : {FName(TEXT("HospitalBed")), FName(TEXT("HospitalBedEmpty"))})
	{
		if (auto* Bed = Page->Get<UImage>(Name))
		{
			const bool bFullBed = Name == TEXT("HospitalBed");
			UTexture2D* Texture = bFull == bFullBed ? FImageUtils::ImportFileAsTexture2D(RokDataPaths::Resolve(
				bFull ? TEXT("Hospital/bed_full.png") : TEXT("Hospital/bed_empty.png"))) : nullptr;
			if (Texture)
			{
				Bed->SetBrushFromTexture(Texture);
			}
			Page->SetShown(Bed, Texture != nullptr);
		}
	}
	if (auto* Mark = Page->Get<UImage>(TEXT("HospitalCivilizationMark")))
	{
		Page->SetShown(Mark, HospitalMark != INDEX_NONE);
		if (HospitalMark != INDEX_NONE)
		{
			Mark->SetBrush(MakeAtlasBrush(LoadUiTexture(CommonAtlasPath),
				SpriteRect(HospitalMark == 0 ? 1074 : 1029, 280, 43, 43), CommonAtlasSize, FVector2D(43)));
		}
	}
	if (auto* State = Page->Get<UTextBlock>(TEXT("HospitalWoundedState")))
	{
		Page->SetShown(State, !bFull);
		State->SetText(HospitalState.bKnown ? NSLOCTEXT("RokHospital", "NoWounded", "没有受伤单位")
			: NSLOCTEXT("RokHospital", "WoundedLoading", "伤员信息尚未同步"));
	}
	if (auto* Title = Page->Get<UTextBlock>(TEXT("HospitalListTitle")))
	{
		Title->SetText(bQueue ? NSLOCTEXT("RokHospital", "InTreatment", "正在治疗的单位")
			: NSLOCTEXT("RokHospital", "ChooseWounded", "选择受伤单位"));
	}
	Page->SetShown(Page->Get<UCanvasPanel>(TEXT("HospitalTreatmentContent")), bFull);
	BindPageAction(*Page, TEXT("HospitalCapacityInfo"), 2000);
	BindPageAction(*Page, TEXT("HospitalHealButton"), 2001);
	BindPageAction(*Page, TEXT("HospitalHealNowButton"), 2002);
	BindPageAction(*Page, TEXT("HospitalMaxButton"), 2003);
	BindPageAction(*Page, TEXT("HospitalExplanationBackButton"), 2004);
	PopulateHospitalTroopRows(*List, RowClass, Units, bQueue);
	return true;
}

void URokUnityFeatureScreenWidget::BuildLegacyHospitalPage(const TArray<FRokHospitalSoldier>& Units, bool bQueue, int32 HospitalMark)
{
	// UI_Win_Hospital uses the 1440x810 UIRoot, centred in the 16:10 capture.
	BuildSplitWindowFrame(FVector2D(174, 63), FVector2D(1092, 697),
		NSLOCTEXT("RokHospital", "TreatmentTitle", "治疗伤员"), FVector2D(1193, 56), nullptr, 1, 0.0f, 30);
	HospitalMainContent = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HospitalMainContent"));
	HospitalMainContent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ModeCanvas, HospitalMainContent, FVector2D::ZeroVector, CurrentExpandedDesignSize, 3);

	const FLinearColor Beige = FLinearColor::FromSRGBColor(FColor(226, 209, 155));
	auto Place = [this](UWidget* Widget, FVector2D Position, FVector2D Size, int32 Layer = 0)
	{
		PlaceOnCanvas(HospitalMainContent, Widget, Position, Size, Layer);
	};
	auto Label = [this, &Place](FName Name, const FText& Text, int32 Size, FLinearColor Color,
		FVector2D Position, FVector2D Extent, ETextJustify::Type Align = ETextJustify::Left, int32 Layer = 4, bool bBold = false)
	{
		auto* Widget = MakeScreenText(Name, Size, Color, false, Align);
		Widget->SetFont(URokMainHudWidget::MakeUnitySourceFont(Size, bBold));
		Widget->SetShadowOffset(FVector2D::ZeroVector);
		Widget->SetShadowColorAndOpacity(FLinearColor::Transparent);
		Widget->SetText(Text);
		Place(Widget, Position, Extent, Layer);
		return Widget;
	};
	auto* Art = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HospitalBed"));
	const bool bFull = !Units.IsEmpty();
	if (auto* Texture = FImageUtils::ImportFileAsTexture2D(RokDataPaths::Resolve(
		bFull ? TEXT("Hospital/bed_full.png") : TEXT("Hospital/bed_empty.png"))))
	{
		Art->SetBrushFromTexture(Texture);
		Art->SetRenderScale(FVector2D(-1.0f, 1.0f));
		Art->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Art, bFull ? FVector2D(225, 175) : FVector2D(206, 315),
			bFull ? FVector2D(431, 437) : FVector2D(426, 248), 1);
	}

	Place(MakeSolidBorder(TEXT("HospitalCapacityBadge"), FLinearColor(0, 0, 0, 0.24f)), {252, 629}, {378, 65});
	Label(TEXT("HospitalCapacityValue"), FText::GetEmpty(), 20, Beige, {312, 652}, {252, 28}, ETextJustify::Center);
	Place(MakeActionButton(TEXT("HospitalCapacityInfo"), 2000, CommonAtlasPath, VipHelpRect,
		CommonAtlasSize, FVector2D(37)), {270, 644}, {37, 37}, 6);
	if (HospitalMark != INDEX_NONE)
	{
		Place(MakeAtlasImage(TEXT("HospitalCivilizationMark"), CommonAtlasPath,
			SpriteRect(HospitalMark == 0 ? 1074 : 1029, 280, 43, 43), CommonAtlasSize, FVector2D(43)),
			{575, 641}, {43, 43}, 4);
	}

	if (Units.IsEmpty())
	{
		Label(TEXT("HospitalWoundedState"), HospitalState.bKnown
			? NSLOCTEXT("RokHospital", "NoWounded", "没有受伤单位")
			: NSLOCTEXT("RokHospital", "WoundedLoading", "伤员信息尚未同步"),
			22, FLinearColor::White, {660, 320}, {564, 35}, ETextJustify::Center);
	}
	else
	{
		Label(TEXT("HospitalListTitle"), bQueue ? NSLOCTEXT("RokHospital", "InTreatment", "正在治疗的单位")
			: NSLOCTEXT("RokHospital", "ChooseWounded", "选择受伤单位"), 20, FLinearColor::White, {658, 143}, {564, 27});
		Place(MakeSolidBorder(TEXT("HospitalTreatmentPanel"), FLinearColor(0, 0, 0, 0.27f)), {659, 173}, {565, 352});
		auto* List = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("HospitalTreatmentList"));
		Place(List, {674, 187}, {532, 324}, 3);
		PopulateHospitalTroopRows(*List, nullptr, Units, bQueue);

		Place(MakeSolidBorder(TEXT("HospitalCostPlate"), FLinearColor(0, 0, 0, .27f)), {659, 537}, {565, 64});
		const FIntRect ResourceIcons[] = {FoodRect, WoodRect, StoneRect, GoldRect};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ResourceIcons); ++Index)
		{
			const FVector2D Position(683 + Index * 122, 553);
			Place(MakeAtlasImage(*FString::Printf(TEXT("HospitalCostIcon_%d"), Index), CommonAtlasPath,
				ResourceIcons[Index], CommonAtlasSize, FVector2D(38)), Position, {38, 38}, 3);
			Label(*FString::Printf(TEXT("HospitalCostValue_%d"), Index), FText::GetEmpty(), 20,
				FLinearColor::FromSRGBColor(FColor(230, 217, 180)), Position + FVector2D(38, 9),
				{96, 28}, ETextJustify::Left, 4, true);
		}
		Label(TEXT("HospitalTreatmentCost"), FText::GetEmpty(), 19, Beige, {670, 558}, {543, 32}, ETextJustify::Center);
		Place(MakeActionButton(TEXT("HospitalHealNowButton"), 2002, CommonAtlasPath, MediumGoldButtonRect,
			CommonAtlasSize, {272, 82}), {659, 613}, {272, 82}, 5);
		Place(MakeActionButton(TEXT("HospitalHealButton"), 2001, CommonAtlasPath, LargeBlueButtonRect,
			CommonAtlasSize, {272, 82}), {952, 613}, {272, 82}, 5);
		Place(MakeActionButton(TEXT("HospitalMaxButton"), 2003, CommonAtlasPath, MediumGoldButtonRect,
			CommonAtlasSize, {272, 82}, NSLOCTEXT("RokHospital", "Maximum", "最大"), 25), {659, 613}, {272, 82}, 5);
		Label(TEXT("HospitalInstantLabel"), NSLOCTEXT("RokHospital", "HealNow", "立即治疗"), 25, FLinearColor::FromSRGBColor(FColor(39, 34, 22)),
			{659, 625}, {272, 35}, ETextJustify::Center, 7, true)->SetVisibility(ESlateVisibility::HitTestInvisible);
		Label(TEXT("HospitalHealLabel"), FText::GetEmpty(), 25, FLinearColor::FromSRGBColor(FColor(17, 37, 52)),
			{952, 625}, {272, 35}, ETextJustify::Center, 7, true)->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(MakeAtlasImage(TEXT("HospitalGemIcon"), CommonAtlasPath, GemRect, CommonAtlasSize, FVector2D(23)),
			{771, 657}, {23, 23}, 7);
		Label(TEXT("HospitalGemCost"), FText::GetEmpty(), 18, FLinearColor::White, {795, 658}, {120, 23}, ETextJustify::Left, 7);
		// RS.CountDownIcon is the angled gold icon used by HospitalMediator.RefreshMaxButton.
		Place(MakeAtlasImage(TEXT("HospitalTimeIcon"), CommonAtlasPath, SpriteRect(340, 32, 42, 46),
			CommonAtlasSize, FVector2D(21, 23)), {1040, 657}, {21, 23}, 7);
		Label(TEXT("HospitalDuration"), FText::GetEmpty(), 18, FLinearColor::White, {1062, 660}, {150, 24}, ETextJustify::Left, 7, true);
	}
	auto* Back = MakeActionButton(TEXT("HospitalExplanationBackButton"), 2004, CommonAtlasPath,
		CommanderBackRect, CommonAtlasSize, FVector2D(69));
	PlaceOnCanvas(ModeCanvas, Back, FVector2D(174, 63), FVector2D(69), 10);
}

void URokUnityFeatureScreenWidget::PopulateHospitalTroopRows(UScrollBox& List, UClass* RowClass,
	const TArray<FRokHospitalSoldier>& Units, bool bQueue)
{
	const FLinearColor Beige = FLinearColor::FromSRGBColor(FColor(226, 209, 155));
	const auto* Catalog = GetGameplayCatalogSubsystem();
	for (const auto& Unit : Units)
	{
		const int32 Id = Unit.Id;
		const FString Key = FString::FromInt(Id);
		FIntRect Icon;
		FRokUnityCatalogTroop Troop;
		const bool bHasIcon = Catalog && Catalog->GetTroop(Id, Troop) && RokTroopPortrait::Resolve(Troop.Icon, Icon);
		FRokTrainArmyDetail Detail;
		FString DetailError;
		const FText Name = RokTrainArmyDetail::Load(Id, Detail, DetailError) && !Detail.Name.IsEmpty()
			? FText::FromString(Detail.Name)
			: FText::Format(NSLOCTEXT("RokHospital", "UnitFallback", "{0}级部队"), FText::AsNumber(Unit.Level));
		const FText UnitText = FText::Format(NSLOCTEXT("RokHospital", "UnitCount", "{0} {1}"), Name, FText::AsNumber(Unit.Count));
		USlider* Slider = nullptr;
		UEditableTextBox* Number = nullptr;
		if (auto* Item = RowClass ? CreateWidget<URokUiPageWidget>(this, RowClass) : nullptr)
		{
			Item->LegacyPrefix = TEXT("Hospital");
			Item->LegacySuffix = TEXT("_") + Key;
			Slider = Item->Get<USlider>(TEXT("Slider"));
			Number = Item->Get<UEditableTextBox>(TEXT("TroopValue"));
			auto* Label = Item->Get<UTextBlock>(TEXT("TroopLabel"));
			auto* Art = Item->Get<UImage>(TEXT("TroopIcon"));
			if (Slider && Number && Label && Art)
			{
				Label->SetText(UnitText);
				Art->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
				if (bHasIcon)
				{
					Art->SetBrush(MakeAtlasBrush(LoadUiTexture(TroopHeadAtlasPath), Icon,
						TroopHeadAtlasSize, FVector2D(88)));
				}
				Slider->SetIsEnabled(!bQueue);
				Number->SetIsReadOnly(bQueue);
				// The Unity font is built at runtime and cannot be serialized into the text box style.
				FEditableTextBoxStyle NumberStyle = Number->GetWidgetStyle();
				NumberStyle.SetFont(URokMainHudWidget::MakeUnitySourceFont(18, false));
				Number->SetWidgetStyle(NumberStyle);
				Item->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				List.AddChild(Item);
			}
			else
			{
				Slider = nullptr;
				Number = nullptr;
			}
		}
		if (!Slider || !Number)
		{
			auto* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *(TEXT("HospitalTroopRow_") + Key));
			Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			auto* Size = WidgetTree->ConstructWidget<USizeBox>();
			Size->SetHeightOverride(101);
			Size->AddChild(Row);
			List.AddChild(Size);
			if (bHasIcon)
			{
				PlaceOnCanvas(Row, MakeAtlasImage(*(TEXT("HospitalTroopIcon_") + Key), TroopHeadAtlasPath,
					Icon, TroopHeadAtlasSize, FVector2D(88)), {0, 0}, {88, 88});
			}
			auto* UnitLabel = MakeScreenText(*(TEXT("HospitalTroopLabel_") + Key), 18, Beige);
			UnitLabel->SetFont(URokMainHudWidget::MakeUnitySourceFont(18, false));
			UnitLabel->SetShadowOffset(FVector2D::ZeroVector);
			UnitLabel->SetShadowColorAndOpacity(FLinearColor::Transparent);
			UnitLabel->SetText(UnitText);
			PlaceOnCanvas(Row, UnitLabel, {94, 12}, {330, 30});
			// UI_Item_HospitalArmyCount/bg spans the slider and its quantity editor.
			PlaceOnCanvas(Row, MakeAtlasBorder(*(TEXT("HospitalQuantityStrip_") + Key), CommonAtlasPath,
				CommonPanelRect, CommonAtlasSize, FVector2D(435, 51), CommonPanelMargin), {94, 37}, {435, 51});
			PlaceOnCanvas(Row, MakeAtlasBorder(*(TEXT("HospitalSliderTrack_") + Key), CommonAtlasPath,
				ResourceSearchSliderBackgroundRect, CommonAtlasSize, FVector2D(343, 23), FMargin(.4f)), {97, 49}, {343, 23});
			PlaceOnCanvas(Row, MakeAtlasBorder(*(TEXT("HospitalSliderFill_") + Key), CommonAtlasPath,
				ResourceSearchSliderFillRect, CommonAtlasSize, FVector2D(HospitalSliderFillWidth, 19), FMargin(.4f)), {97, 51}, {HospitalSliderFillWidth, 19}, 1);
			Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), *(TEXT("HospitalSlider_") + Key));
			Slider->SetIndentHandle(false);
			Slider->SetIsEnabled(!bQueue);
			FSlateBrush Empty;
			Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
			const FSlateBrush Thumb = MakeAtlasBrush(LoadUiTexture(CommonAtlasPath), ResourceSearchSliderThumbRect,
				CommonAtlasSize, FVector2D(20, 37));
			FSliderStyle SliderStyle;
			SliderStyle.SetNormalBarImage(Empty).SetHoveredBarImage(Empty).SetDisabledBarImage(Empty);
			SliderStyle.SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDisabledThumbImage(Thumb);
			Slider->SetWidgetStyle(SliderStyle);
			PlaceOnCanvas(Row, Slider, {97, 42}, {343, 37}, 3);
			Number = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), *(TEXT("HospitalTroopValue_") + Key));
			Number->SetJustification(ETextJustify::Center);
			Number->SetIsReadOnly(bQueue);
			FEditableTextBoxStyle NumberStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
			NumberStyle.SetBackgroundImageNormal(Empty).SetBackgroundImageHovered(Empty)
				.SetBackgroundImageFocused(Empty).SetBackgroundImageReadOnly(Empty);
			NumberStyle.SetFont(URokMainHudWidget::MakeUnitySourceFont(18, false))
				.SetPadding(FMargin(0))
				.SetForegroundColor(FSlateColor(FLinearColor::White))
				.SetReadOnlyForegroundColor(FSlateColor(FLinearColor::White));
			Number->SetWidgetStyle(NumberStyle);
			PlaceOnCanvas(Row, Number, {446, 37}, {73, 51}, 3);
		}

		if (!HospitalCounts.Contains(Id)) HospitalCounts.Add(Id, Unit.Count);
		auto* Quantity = NewObject<URokHospitalQuantity>(this);
		HospitalQuantities.Add(Id, Quantity);
		Quantity->Bind(Unit.Count, Slider, Number, [this, Id, Maximum = Unit.Count](int32 Count)
		{
			if (ActiveMode != ERokUnityFeatureScreenMode::Hospital || HospitalState.FinishTime > 0) return;
			const int32 Selected = FMath::Clamp(Count, 0, Maximum);
			HospitalCounts.Add(Id, Selected);
			if (auto* Quantity = HospitalQuantities.FindRef(Id).Get()) Quantity->SetCount(Selected);
			UpdateHospitalActions();
		});
		HospitalCounts[Id] = FMath::Clamp(HospitalCounts[Id], 0, Unit.Count);
		Quantity->SetCount(bQueue ? Unit.Count : HospitalCounts[Id]);
	}
}

void URokUnityFeatureScreenWidget::SetHospitalExplanationVisible(bool bVisible)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Hospital) return;
	bHospitalExplanationVisible = bVisible && HospitalExplanationWidget;
	if (HospitalMainContent)
		HospitalMainContent->SetVisibility(bHospitalExplanationVisible ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (HospitalExplanationWidget)
		HospitalExplanationWidget->SetVisibility(bHospitalExplanationVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (auto* Back = GetWidgetFromName(TEXT("HospitalExplanationBackButton")))
		Back->SetVisibility(bHospitalExplanationVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}


void URokUnityFeatureScreenWidget::RefreshHospital()
{
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	const auto Role = ReadHospitalDisplayRole(Net, GetHospitalPreviewState());
	if (FRokHospitalState::Read(Role) != HospitalState)
	{
		RebuildModeContent(false);
		return;
	}
	UpdateHospitalActions();
}

void URokUnityFeatureScreenWidget::UpdateHospitalActions()
{
	auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	const EHospitalLocalPreview Preview = GetHospitalPreviewState();
	const auto Role = ReadHospitalDisplayRole(Net, Preview);
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	const bool bQueue = HospitalState.FinishTime > 0;
	double Now = 0;
	bool bClockKnown = Net && Net->TryGetServerTimeSeconds(Now);
	if (Preview == EHospitalLocalPreview::Treating || Preview == EHospitalLocalPreview::Ready)
	{
		Now = Preview == EHospitalLocalPreview::Treating ? HospitalPreviewFinishTime - 280 : HospitalPreviewFinishTime;
		bClockKnown = true;
	}
	const int64 Remaining = bQueue && bClockKnown ? FMath::Max<int64>(0, FMath::CeilToInt64(HospitalState.FinishTime - Now)) : 0;
	const bool bReady = bQueue && bClockKnown && Remaining == 0 && !HospitalState.InTreatment.IsEmpty();
	const bool bPending = Net && Net->IsHospitalTreatmentPending();

	TMap<int32, int32> Counts;
	const auto& Units = bQueue ? HospitalState.InTreatment : HospitalState.Wounded;
	for (const auto& Unit : Units)
	{
		const int32 Count = bQueue ? Unit.Count : HospitalCounts.FindRef(Unit.Id);
		if (!bQueue && Count > 0) Counts.Add(Unit.Id, Count);
		if (auto* Fill = GetWidgetFromName(*FString::Printf(TEXT("HospitalSliderFill_%d"), Unit.Id)))
			if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(Fill->Slot))
				CanvasSlot->SetSize(FVector2D(HospitalSliderFillWidth * Count / Unit.Count, 19));
	}
	const auto Quote = Config ? FRokHospitalQuote::Calculate(*Config, Counts, Role, Remaining) : FRokHospitalQuote{};
	const bool bShowMax = !bQueue && Counts.IsEmpty();
	auto Show = [this](FName Name, bool bVisible, bool bInteractive = false)
	{
		if (auto* Widget = GetWidgetFromName(Name))
			Widget->SetVisibility(!bVisible ? ESlateVisibility::Collapsed
				: bInteractive ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
	};
	auto SetText = [this](FName Name, const FText& Text)
	{
		if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(Name))) Label->SetText(Text);
	};
	auto Enable = [this](FName Name, bool bEnabled)
	{
		if (auto* Widget = GetWidgetFromName(Name)) Widget->SetIsEnabled(bEnabled);
	};
	int64 Capacity = 0;
	auto* Catalog = GetGameplayCatalogSubsystem();
	FRokUnityCatalogBuildingType HospitalType;
	if (Catalog && Catalog->GetBuildingType(14, HospitalType))
	{
		const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
		if (City && City->HasAppliedServerCity())
		{
			for (const FRokServerBuilding& Building : City->GetAppliedBuildings())
				if (Building.Type == 14 && Building.Level > 0)
					Capacity += HospitalType.GetHospitalCapacity(Building.Level);
		}
		else if (FeatureContextActor.IsValid())
		{
			auto* Buildings = GetWorld() ? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
			if (Buildings)
			{
				FRokBuildingState Building;
				if (!Buildings->TryPeekExistingBuildingStateForActor(FeatureContextActor.Get(), Building))
					Building = Buildings->GetBuildingStateForActor(FeatureContextActor.Get());
				Capacity = HospitalType.GetHospitalCapacity(Building.Level);
			}
		}
		int64 Bonus = 0;
		if (RokSprotoJson::TryGetInt64(Role, TEXT("hospitalSpaceMulti"), Bonus))
			Capacity = FMath::FloorToInt64(Capacity * FMath::Max(0.0, 1.0 + Bonus / 1000.0));
	}
	int64 Wounded = 0;
	for (const auto& Unit : HospitalState.Wounded) Wounded += Unit.Count;
	if (bQueue)
		for (const auto& Unit : HospitalState.InTreatment) Wounded -= Unit.Count;
	SetText(TEXT("HospitalCapacityValue"), FText::Format(NSLOCTEXT("RokHospital", "Capacity", "{0}/{1}"),
		HospitalState.bKnown ? FText::AsNumber(FMath::Max<int64>(0, Wounded)) : FText::FromString(TEXT("--")),
		Capacity > 0 ? FText::AsNumber(Capacity) : FText::FromString(TEXT("--"))));

	Show(TEXT("HospitalMaxButton"), bShowMax, true);
	Show(TEXT("HospitalHealNowButton"), !bReady && !bShowMax, true);
	Show(TEXT("HospitalInstantLabel"), !bReady && !bShowMax);
	Show(TEXT("HospitalGemIcon"), !bReady && !bShowMax && Quote.bValid);
	Show(TEXT("HospitalGemCost"), !bReady && !bShowMax && Quote.bValid);
	Show(TEXT("HospitalTimeIcon"), !bQueue && !bShowMax && Quote.bValid);
	Show(TEXT("HospitalDuration"), !bQueue && !bShowMax && Quote.bValid);
	Enable(TEXT("HospitalHealButton"), !bPending && (bReady || (!bQueue && Quote.bValid && Quote.bHasResources)));
	Enable(TEXT("HospitalHealNowButton"), !bPending && Quote.bValid && Quote.bHasGems);
	Enable(TEXT("HospitalMaxButton"), !bPending);
	SetText(TEXT("HospitalHealLabel"), bReady ? NSLOCTEXT("RokHospital", "Collect", "领取")
		: bQueue ? NSLOCTEXT("RokHospital", "Healing", "治疗中") : NSLOCTEXT("RokHospital", "Heal", "治疗"));
	SetText(TEXT("HospitalGemCost"), FText::AsNumber(Quote.Gems));
	auto Duration = [](int64 Seconds)
	{
		return FText::FromString(FString::Printf(TEXT("%02lld:%02lld:%02lld"), Seconds / 3600, (Seconds / 60) % 60, Seconds % 60));
	};
	SetText(TEXT("HospitalDuration"), Duration(Quote.Seconds));
	Show(TEXT("HospitalTreatmentCost"), bQueue);
	SetText(TEXT("HospitalTreatmentCost"), bReady ? NSLOCTEXT("RokHospital", "QueueReady", "治疗完成")
		: bClockKnown ? Duration(Remaining) : NSLOCTEXT("RokHospital", "AwaitClock", "正在同步治疗时间"));

	int32 VisibleResources = 0;
	for (int64 Amount : Quote.Resources) if (Amount > 0) ++VisibleResources;
	int32 VisibleIndex = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Quote.Resources); ++Index)
	{
		const FName IconName(*FString::Printf(TEXT("HospitalCostIcon_%d"), Index));
		const FName ValueName(*FString::Printf(TEXT("HospitalCostValue_%d"), Index));
		const bool bVisible = !bQueue && Quote.Resources[Index] > 0;
		Show(IconName, bVisible);
		Show(ValueName, bVisible);
		if (bVisible)
		{
			const FVector2D Position(947 - 65 * VisibleResources + 130 * VisibleIndex++, 552);
			if (auto* Icon = GetWidgetFromName(IconName))
				if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(Icon->Slot)) CanvasSlot->SetPosition(Position);
			if (auto* Value = GetWidgetFromName(ValueName))
				if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(Value->Slot)) CanvasSlot->SetPosition(Position + FVector2D(38, 9));
			SetText(ValueName, FText::AsNumber(Quote.Resources[Index]));
		}
	}
	HospitalQuote = Quote;
	HospitalSelectedCounts = MoveTemp(Counts);
	bHospitalCanCollect = bReady;
}

void URokUnityFeatureScreenWidget::OpenHospitalInstantConfirmation()
{
	if (HospitalConfirmation || bHospitalExplanationVisible) return;
	UpdateHospitalActions();
	if (!HospitalQuote.bValid || !HospitalQuote.bHasGems) return;
	HospitalConfirmationPrice = HospitalQuote.Gems;
	const FText Message = FText::Format(NSLOCTEXT("RokHospital", "ConfirmGems", "消耗 {0} 宝石立即治疗？"),
		FText::AsNumber(HospitalConfirmationPrice));
	UClass* ConfirmClass = ActiveModePage ? ResolveFeaturePageClass(TEXT("HospitalConfirm")) : nullptr;
	if (auto* Dialog = ConfirmClass ? CreateWidget<URokUiPageWidget>(this, ConfirmClass) : nullptr)
	{
		if (auto* Text = Dialog->Get<UTextBlock>(TEXT("HospitalConfirmationText")); Text
			&& Dialog->Get<URokUnityIndexedButton>(TEXT("HospitalInstantCancel"))
			&& Dialog->Get<URokUnityIndexedButton>(TEXT("HospitalInstantConfirm")))
		{
			Text->SetText(Message);
			BindPageAction(*Dialog, TEXT("HospitalInstantCancel"), 2005);
			BindPageAction(*Dialog, TEXT("HospitalInstantConfirm"), 2006);
			Dialog->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			PlaceOnCanvas(ModeCanvas, Dialog, FVector2D::ZeroVector, CurrentExpandedDesignSize, 90);
			HospitalConfirmation = Dialog;
			return;
		}
	}

	auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HospitalConfirmation"));
	HospitalConfirmation = Canvas;
	PlaceOnCanvas(ModeCanvas, Canvas, FVector2D::ZeroVector, CurrentExpandedDesignSize, 90);
	auto* Shade = MakeSolidBorder(TEXT("HospitalConfirmationMask"), FLinearColor(0, 0, 0, .68f));
	Shade->SetVisibility(ESlateVisibility::Visible);
	PlaceOnCanvas(Canvas, Shade, FVector2D::ZeroVector, CurrentExpandedDesignSize);
	const FVector2D Size(538, 200);
	const FVector2D Origin = (CurrentExpandedDesignSize - Size) * .5f;
	PlaceOnCanvas(Canvas, MakeAtlasBorder(TEXT("HospitalConfirmationFrame"), FrameAtlasPath,
		WindowBodyRect, FrameAtlasSize, Size, WindowBodyMargin), Origin, Size, 1);
	auto* Text = MakeScreenText(TEXT("HospitalConfirmationText"), 22, FLinearColor::White, false, ETextJustify::Center);
	Text->SetText(Message);
	PlaceOnCanvas(Canvas, Text, Origin + FVector2D(20, 50), FVector2D(498, 40), 2);
	PlaceOnCanvas(Canvas, MakeActionButton(TEXT("HospitalInstantCancel"), 2005,
		CommonAtlasPath, MediumBlueButtonRect, CommonAtlasSize, FVector2D(175, 62),
		NSLOCTEXT("RokHospital", "Cancel", "取消"), 22), Origin + FVector2D(74, 120), FVector2D(175, 62), 3);
	PlaceOnCanvas(Canvas, MakeActionButton(TEXT("HospitalInstantConfirm"), 2006,
		CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize, FVector2D(175, 62),
		NSLOCTEXT("RokHospital", "Confirm", "确认"), 22), Origin + FVector2D(289, 120), FVector2D(175, 62), 3);
}

void URokUnityFeatureScreenWidget::SubmitHospitalTreatment(bool bImmediately)
{
	auto* Instance = GetGameInstance();
	auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || bHospitalExplanationVisible || Net->IsHospitalTreatmentPending()) return;
	UpdateHospitalActions();
	const bool bCollect = !bImmediately && bHospitalCanCollect;
	if (bImmediately && (!HospitalQuote.bValid || !HospitalQuote.bHasGems || HospitalQuote.Gems > HospitalConfirmationPrice))
	{
		ShowBoundaryFeedback(NSLOCTEXT("RokHospital", "PriceChanged", "治疗费用已变更，请重新确认。"));
		return;
	}
	if (!bImmediately && !bCollect && (!HospitalQuote.bValid || !HospitalQuote.bHasResources || HospitalSelectedCounts.IsEmpty()))
		return;
	FString Error;
	const auto Context = FeatureContextActor;
	if (!Net->RequestHospitalTreatment(HospitalSelectedCounts, bImmediately, bCollect,
		[WeakThis = TWeakObjectPtr<URokUnityFeatureScreenWidget>(this), Context](const FString& Result)
		{
			auto* Widget = WeakThis.Get();
			if (!Widget || Widget->ActiveMode != ERokUnityFeatureScreenMode::Hospital || Widget->FeatureContextActor != Context) return;
			if (Result.IsEmpty()) Widget->CloseScreen();
			else Widget->ShowBoundaryFeedback(FText::FromString(Result));
		}, Error))
	{
		ShowBoundaryFeedback(FText::FromString(Error));
	}
	UpdateHospitalActions();
}


void URokUnityFeatureScreenWidget::BuildCityManager()
{
	struct FCityManagerGroupPresentation
	{
		int32 GroupId;
		int32 SeriesId;
		const TCHAR* Name;
		const TCHAR* Description;
		const TCHAR* AtlasPath;
		FIntRect IconRect;
		FIntPoint AtlasSize;
		int32 Function;
	};
	static const FText SeriesLabels[] = {
		NSLOCTEXT("RokCityManager", "Relocate", "迁城"),
		NSLOCTEXT("RokCityManager", "BattleFever", "战争狂热"),
		NSLOCTEXT("RokCityManager", "CityDefense", "城市防御"),
		NSLOCTEXT("RokCityManager", "BattleBoosts", "战斗增益"),
		NSLOCTEXT("RokCityManager", "MilitaryStrategy", "军事策略"),
		NSLOCTEXT("RokCityManager", "ResourceEconomy", "资源经济")};
	static const FCityManagerGroupPresentation Groups[] = {
		{101, 1, TEXT("城市迁移"), TEXT("将您的城市迁移至地图的其他区域。（与集结部队战斗时无法使用）"), Item5AtlasPath, SpriteRect(385, 641, 126, 126), Item5AtlasSize, 1},
		{201, 2, TEXT("战争狂热"), TEXT("开展军事行动将激活战争狂热效果，一段时间内将无法启用城市护盾"), Item5AtlasPath, SpriteRect(1, 1921, 126, 126), Item5AtlasSize, 2},
		{301, 3, TEXT("城市护盾"), TEXT("保护您的城市不受其他城主的攻击与侦察"), Item3AtlasPath, SpriteRect(1, 769, 126, 126), Item3AtlasSize, 0},
		{401, 4, TEXT("攻击强化"), TEXT("提升部队的攻击力"), Item3AtlasPath, SpriteRect(1, 641, 126, 126), Item3AtlasSize, 0},
		{402, 4, TEXT("防御强化"), TEXT("提升部队的防御力"), Item3AtlasPath, SpriteRect(1, 513, 126, 126), Item3AtlasSize, 0},
		{403, 4, TEXT("反侦察"), TEXT("防止敌方斥候在侦察时获取关于您城市的情报"), Item3AtlasPath, SpriteRect(129, 769, 126, 126), Item3AtlasSize, 0},
		{501, 5, TEXT("部队容量"), TEXT("提升部队派遣时的容量上限"), Item3AtlasPath, SpriteRect(129, 513, 126, 126), Item3AtlasSize, 0},
		{502, 5, TEXT("疑兵"), TEXT("让敌方斥候所获取的关于您城市的情报中的兵力数量增加"), Item3AtlasPath, SpriteRect(129, 641, 126, 126), Item3AtlasSize, 0},
		{601, 6, TEXT("采集速度"), TEXT("提升地图上资源的采集速度"), Item3AtlasPath, SpriteRect(129, 897, 126, 126), Item3AtlasSize, 0},
		{602, 6, TEXT("食物产量"), TEXT("提升城市中的食物产量"), Item3AtlasPath, SpriteRect(1, 385, 126, 126), Item3AtlasSize, 0},
		{603, 6, TEXT("木材产量"), TEXT("提升城市中的木材产量"), Item3AtlasPath, SpriteRect(1, 257, 126, 126), Item3AtlasSize, 0},
		{604, 6, TEXT("铁矿石产量"), TEXT("提升城市中的铁矿石产量"), Item3AtlasPath, SpriteRect(1, 129, 126, 126), Item3AtlasSize, 0},
		{605, 6, TEXT("金币产量"), TEXT("提升城市中的金币产量"), Item3AtlasPath, SpriteRect(1, 1, 126, 126), Item3AtlasSize, 0}};

	UClass* GroupClass = ResolveFeaturePageClass(TEXT("CityManagerGroupRow"));
	UClass* WarLevelClass = ResolveFeaturePageClass(TEXT("CityManagerWarLevelRow"));
	UClass* DetailClass = ResolveFeaturePageClass(TEXT("CityManagerDetailRow"));
	const URokUiPageWidget* Page = GroupClass && WarLevelClass && DetailClass ? MountModePage() : nullptr;
	const auto ApplyIcon = [this](UImage* Image, const TCHAR* Path, const FIntRect& Rect, const FIntPoint& AtlasSize)
	{
		if (!Image) return;
		UTexture2D* Texture = LoadUiTexture(Path);
		if (!Texture)
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
			return;
		}
		FSlateBrush Brush = Image->GetBrush();
		const FSlateBrush AtlasBrush = MakeAtlasBrush(Texture, Rect, AtlasSize, Brush.GetImageSize());
		Brush.SetResourceObject(Texture);
		Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
		Image->SetBrush(Brush);
	};
	if (Page)
	{
		const bool bMain = SelectedCityManagerPage == 0;
		const bool bWar = SelectedCityManagerPage == 2;
		Page->SetPresent(Page->Get<UScrollBox>(TEXT("CityManagerListScroll")), bMain);
		for (const FName Name : {FName(TEXT("CityManagerWarDescriptionPanel")), FName(TEXT("CityManagerWarDescription")),
			FName(TEXT("CityManagerWarState")), FName(TEXT("CityManagerWarLevelScroll"))})
		{
			Page->SetPresent(Page->Get<UWidget>(Name), bWar);
		}
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("CityManagerDetailTitle")), !bMain && !bWar);
		Page->SetPresent(Page->Get<UScrollBox>(TEXT("CityManagerDetailScroll")), !bMain && !bWar);
		Page->SetPresent(Page->Get<UButton>(TEXT("CityManagerBack")), !bMain);
		if (!bMain)
		{
			BindPageAction(*Page, TEXT("CityManagerBack"), 2100);
		}
	}
	else
	{
		BuildSplitWindowFrame(
			FVector2D(160.0f, 55.0f),
			FVector2D(1120.0f, 700.0f),
			NSLOCTEXT("RokCityManager", "Title", "城市管理"),
			FVector2D(1191.5f, 54.5f));
		if (SelectedCityManagerPage != 0)
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeActionButton(
					TEXT("CityManagerBack"),
					2100,
					CommonAtlasPath,
					CityManagerBackRect,
					CommonAtlasSize,
					FVector2D(69.0f)),
				FVector2D(179.5f, 54.5f),
				FVector2D(69.0f),
				10);
		}
	}

	if (SelectedCityManagerPage == 0)
	{
		if (Page)
		{
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Groups); ++Index)
			{
				const FCityManagerGroupPresentation& Group = Groups[Index];
				UCanvasPanel* GroupSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("CityManagerGroupSlot_%d"), Group.GroupId));
				if (const URokUiPageWidget* Row = MountPageItem(GroupSlot, GroupClass, TEXT("CityManager"), Group.GroupId))
				{
					SetFeaturePageText(*Row, TEXT("GroupName"), FText::FromString(Group.Name));
					SetFeaturePageText(*Row, TEXT("GroupDescription"), FText::FromString(Group.Description));
					ApplyIcon(Row->Get<UImage>(TEXT("GroupIcon")), Group.AtlasPath, Group.IconRect, Group.AtlasSize);
					const bool bRelocate = Group.Function == 1;
					Row->SetPresent(Row->Get<UButton>(TEXT("RelocateButton")), bRelocate);
					Row->SetPresent(Row->Get<UButton>(TEXT("GroupButton")), !bRelocate);
					Row->SetPresent(Row->Get<UImage>(TEXT("GroupArrow")), !bRelocate);
					if (bRelocate)
					{
						BindPageAction(*Row, TEXT("RelocateButton"), 2101);
					}
					else
					{
						BindPageAction(*Row, TEXT("GroupButton"), 2102 + Index);
					}
				}
			}
			UE_LOG(LogTemp, Display, TEXT("Rok city manager: page=main series=6 groups=13 authority=config_only."));
			return;
		}

		UScrollBox* ListScroll = WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(), TEXT("CityManagerListScroll"));
		ListScroll->SetOrientation(Orient_Vertical);
		ListScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		ListScroll->SetAllowOverscroll(true);
		ListScroll->SetAnimateWheelScrolling(true);
		ListScroll->SetWheelScrollMultiplier(12.0f);
		PlaceOnCanvas(
			ModeCanvas, ListScroll, FVector2D(208.0f, 131.16f),
			FVector2D(1024.0f, 597.83f), 4);
		UCanvasPanel* ListContent = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("CityManagerListContent"));
		ListScroll->AddChild(WrapFixedSize(
			ListContent, TEXT("CityManagerListContentSize"),
			FVector2D(1024.0f, 2180.0f)));

		float RowY = 0.0f;
		int32 GroupIndex = 0;
		for (int32 SeriesIndex = 0; SeriesIndex < 6; ++SeriesIndex)
		{
			PlaceOnCanvas(
				ListContent,
				MakeAtlasImage(
					*FString::Printf(TEXT("CityManagerSeriesBackground_%d"), SeriesIndex),
					CommonAtlasPath, ArmyShowRuleBarRect, CommonAtlasSize,
					FVector2D(1024.0f, 60.0f)),
				FVector2D(0.0f, RowY), FVector2D(1024.0f, 60.0f), 0);
			UTextBlock* SeriesTitle = MakeScreenText(
				*FString::Printf(TEXT("CityManagerSeriesTitle_%d"), SeriesIndex),
				23, FLinearColor(0.96f, 0.89f, 0.63f, 1.0f), true);
			SeriesTitle->SetText(SeriesLabels[SeriesIndex]);
			PlaceOnCanvas(
				ListContent, SeriesTitle, FVector2D(28.0f, RowY + 12.0f),
				FVector2D(920.0f, 40.0f), 2);
			RowY += 60.0f;

			for (const FCityManagerGroupPresentation& Group : Groups)
			{
				if (Group.SeriesId != SeriesIndex + 1)
				{
					continue;
				}
				UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(
					UCanvasPanel::StaticClass(),
					*FString::Printf(TEXT("CityManagerGroupRow_%d"), Group.GroupId));
				PlaceOnCanvas(
					Row,
					MakeAtlasBorder(
						*FString::Printf(TEXT("CityManagerGroupBackground_%d"), Group.GroupId),
						CommonAtlasPath, TaskRowRect, CommonAtlasSize,
						FVector2D(1024.0f, 140.0f), TaskRowMargin),
					FVector2D::ZeroVector, FVector2D(1024.0f, 140.0f), 0);
				PlaceOnCanvas(
					Row,
					MakeAtlasImage(
						*FString::Printf(TEXT("CityManagerGroupIconFrame_%d"), Group.GroupId),
						CommonAtlasPath, CityManagerIconFrameRect, CommonAtlasSize,
						FVector2D(90.0f)),
					FVector2D(916.0f, 25.0f), FVector2D(90.0f), 2);
				PlaceOnCanvas(
					Row,
					MakeAtlasImage(
						*FString::Printf(TEXT("CityManagerGroupIcon_%d"), Group.GroupId),
						Group.AtlasPath, Group.IconRect, Group.AtlasSize,
						FVector2D(82.0f)),
					FVector2D(920.0f, 29.0f), FVector2D(82.0f), 3);
				UTextBlock* Name = MakeScreenText(
					*FString::Printf(TEXT("CityManagerGroupName_%d"), Group.GroupId),
					23, FLinearColor(0.96f, 0.89f, 0.63f, 1.0f), true,
					ETextJustify::Right);
				Name->SetText(FText::FromString(Group.Name));
				PlaceOnCanvas(Row, Name, FVector2D(570.0f, 22.0f), FVector2D(325.0f, 38.0f), 3);
				UTextBlock* Description = MakeScreenText(
					*FString::Printf(TEXT("CityManagerGroupDescription_%d"), Group.GroupId),
					17, FLinearColor(0.82f, 0.82f, 0.78f, 1.0f), false,
					ETextJustify::Right);
				Description->SetAutoWrapText(true);
				Description->SetText(FText::FromString(Group.Description));
				PlaceOnCanvas(Row, Description, FVector2D(280.0f, 66.0f), FVector2D(615.0f, 54.0f), 3);
				if (Group.Function == 1)
				{
					PlaceOnCanvas(
						Row,
						MakeActionButton(
							TEXT("CityManagerRelocateButton"), 2101,
							CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize,
							FVector2D(138.0f, 54.0f),
							NSLOCTEXT("RokCityManager", "RelocateAction", "迁城"), 18,
							FLinearColor(0.04f, 0.14f, 0.24f, 1.0f),
							ESlateBrushDrawType::Box, MiniBlueMargin),
						FVector2D(37.0f, 43.0f), FVector2D(138.0f, 54.0f), 8);
				}
				else
				{
					const int32 GroupActionId = Group.GroupId == 201
						? 2103
						: 2104 + (GroupIndex - 2);
					PlaceOnCanvas(
						Row,
						MakeAtlasImage(
							*FString::Printf(TEXT("CityManagerGroupArrow_%d"), Group.GroupId),
							MailAtlasPath, CityManagerArrowRect, MailAtlasSize,
							FVector2D(28.0f, 17.0f)),
						FVector2D(54.0f, 61.0f), FVector2D(28.0f, 17.0f), 4);
					PlaceOnCanvas(
						Row,
						MakeActionButton(
							*FString::Printf(TEXT("CityManagerGroupButton_%d"), Group.GroupId),
							GroupActionId, nullptr, FIntRect(), FIntPoint(1, 1),
							FVector2D(1024.0f, 140.0f)),
						FVector2D::ZeroVector, FVector2D(1024.0f, 140.0f), 9);
				}
				PlaceOnCanvas(ListContent, Row, FVector2D(0.0f, RowY), FVector2D(1024.0f, 140.0f), 1);
				RowY += 140.0f;
				++GroupIndex;
			}
		}
		UE_LOG(LogTemp, Display, TEXT("Rok city manager: page=main series=6 groups=13 authority=config_only."));
		return;
	}

	if (SelectedCityManagerPage == 2)
	{
		static const TCHAR* LevelLabels[] = {
			TEXT("10-12"), TEXT("13-14"), TEXT("15"), TEXT("16"), TEXT("17"),
			TEXT("18"), TEXT("19"), TEXT("20"), TEXT("21"), TEXT("22"),
			TEXT("23"), TEXT("24"), TEXT("25")};
		static const TCHAR* DurationLabels[] = {
			TEXT("60秒"), TEXT("2分钟"), TEXT("3分钟"), TEXT("4分钟"), TEXT("5分钟"),
			TEXT("6分钟"), TEXT("7分钟"), TEXT("8分钟"), TEXT("9分钟"), TEXT("10分钟"),
			TEXT("11分钟"), TEXT("12分钟"), TEXT("15分钟")};
		if (Page)
		{
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(LevelLabels); ++Index)
			{
				UCanvasPanel* LevelSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("CityManagerWarSlot_%d"), Index));
				if (const URokUiPageWidget* Row = MountPageItem(LevelSlot, WarLevelClass, TEXT("CityManagerWar"), Index))
				{
					SetFeaturePageText(*Row, TEXT("Level"), FText::FromString(LevelLabels[Index]));
					SetFeaturePageText(*Row, TEXT("Duration"), FText::FromString(DurationLabels[Index]));
				}
			}
			UE_LOG(LogTemp, Display, TEXT("Rok city manager: page=war_frenzy authority=server_unsynced state=inactive."));
			return;
		}

		PlaceOnCanvas(
			ModeCanvas, MakeAtlasBorder(
				TEXT("CityManagerWarDescriptionPanel"), CommonAtlasPath,
				CommonPanelRect, CommonAtlasSize, FVector2D(814.0f, 171.45f),
				CommonPanelMargin),
			FVector2D(313.0f, 147.0f), FVector2D(814.0f, 171.45f), 3);
		UTextBlock* Description = MakeScreenText(
			TEXT("CityManagerWarDescription"), 19,
			FLinearColor(0.89f, 0.88f, 0.82f, 1.0f), false,
			ETextJustify::Center);
		Description->SetAutoWrapText(true);
		Description->SetText(NSLOCTEXT(
			"RokCityManager", "WarFrenzyDescription",
			"开展军事行动会激活战争狂热，期间提升部队攻击力，但无法开启城市护盾。\n战争狂热只对市政厅等级10级以上的城主生效。"));
		PlaceOnCanvas(
			ModeCanvas, Description, FVector2D(343.0f, 177.0f),
			FVector2D(754.0f, 112.0f), 5);
		UTextBlock* State = MakeScreenText(
			TEXT("CityManagerWarState"), 20,
			FLinearColor(0.96f, 0.86f, 0.46f, 1.0f), true,
			ETextJustify::Center);
		State->SetText(NSLOCTEXT(
			"RokCityManager", "WarFrenzyInactive", "战争狂热（未激活）"));
		PlaceOnCanvas(
			ModeCanvas, State, FVector2D(586.15f, 331.0f),
			FVector2D(267.7f, 30.0f), 5);
		UScrollBox* WarScroll = WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(), TEXT("CityManagerWarLevelScroll"));
		WarScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		WarScroll->SetOrientation(Orient_Vertical);
		PlaceOnCanvas(ModeCanvas, WarScroll, FVector2D(208.0f, 361.0f), FVector2D(1024.0f, 368.0f), 4);
		UCanvasPanel* WarContent = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CityManagerWarLevelContent"));
		WarScroll->AddChild(WrapFixedSize(WarContent, TEXT("CityManagerWarLevelContentSize"), FVector2D(1024.0f, 624.0f)));
		for (int32 Index = 0; Index < 13; ++Index)
		{
			const float Y = Index * 48.0f;
			PlaceOnCanvas(WarContent, MakeAtlasBorder(*FString::Printf(TEXT("CityManagerWarLevelBackground_%d"), Index), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(1024.0f, 47.0f), CommonPanelMargin), FVector2D(0.0f, Y), FVector2D(1024.0f, 47.0f), 0);
			UTextBlock* Level = MakeScreenText(*FString::Printf(TEXT("CityManagerWarLevel_%d"), Index), 17, FLinearColor(0.88f, 0.86f, 0.78f, 1.0f), false, ETextJustify::Center);
			Level->SetText(FText::FromString(LevelLabels[Index]));
			PlaceOnCanvas(WarContent, Level, FVector2D(80.0f, Y + 9.0f), FVector2D(360.0f, 30.0f), 2);
			UTextBlock* Duration = MakeScreenText(*FString::Printf(TEXT("CityManagerWarDuration_%d"), Index), 17, FLinearColor(0.95f, 0.87f, 0.54f, 1.0f), false, ETextJustify::Center);
			Duration->SetText(FText::FromString(DurationLabels[Index]));
			PlaceOnCanvas(WarContent, Duration, FVector2D(584.0f, Y + 9.0f), FVector2D(360.0f, 30.0f), 2);
		}
		UE_LOG(LogTemp, Display, TEXT("Rok city manager: page=war_frenzy authority=server_unsynced state=inactive."));
		return;
	}

	const FCityManagerGroupPresentation* SelectedGroup = nullptr;
	for (const FCityManagerGroupPresentation& Group : Groups)
	{
		if (Group.GroupId == SelectedCityManagerGroupId)
		{
			SelectedGroup = &Group;
			break;
		}
	}
	if (!SelectedGroup)
	{
		SelectedCityManagerPage = 0;
		RebuildModeContent();
		return;
	}
	UScrollBox* DetailScroll = nullptr;
	UCanvasPanel* DetailContent = nullptr;
	if (Page)
	{
		SetFeaturePageText(*Page, TEXT("CityManagerDetailTitle"), FText::FromString(SelectedGroup->Name));
		DetailScroll = Page->Get<UScrollBox>(TEXT("CityManagerDetailScroll"));
		DetailContent = Page->Get<UCanvasPanel>(TEXT("CityManagerDetailContent"));
	}
	else
	{
		UTextBlock* DetailTitle = MakeScreenText(
			TEXT("CityManagerDetailTitle"), 28,
			FLinearColor(0.96f, 0.88f, 0.58f, 1.0f), true,
			ETextJustify::Center);
		DetailTitle->SetText(FText::FromString(SelectedGroup->Name));
		PlaceOnCanvas(ModeCanvas, DetailTitle, FVector2D(208.0f, 132.0f), FVector2D(1024.0f, 44.0f), 5);
		DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(), TEXT("CityManagerDetailScroll"));
		DetailScroll->SetOrientation(Orient_Vertical);
		DetailScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		PlaceOnCanvas(ModeCanvas, DetailScroll, FVector2D(208.0f, 182.0f), FVector2D(1024.0f, 547.0f), 4);
		DetailContent = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CityManagerDetailContent"));
	}
	if (!DetailScroll || !DetailContent) return;

	URokUnityConfigSubsystem* Config = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	URokUnityEconomyCatalogSubsystem* Economy = GetEconomyCatalogSubsystem();
	if (Economy && !Economy->IsCatalogLoaded())
	{
		Economy->LoadCatalog();
	}
	TArray<FRokUnityEconomyItem> DetailItems;
	if (Config)
	{
		if (!Config->IsManifestLoaded())
		{
			Config->LoadManifest();
		}
		if (const TArray<TSharedPtr<FJsonObject>>* Rows = Config->GetTableRows(TEXT("CityBuff")))
		{
			for (const TSharedPtr<FJsonObject>& Row : *Rows)
			{
				double GroupNumber = 0.0;
				double ItemNumber = 0.0;
				if (!Row.IsValid()
					|| !Row->TryGetNumberField(TEXT("group"), GroupNumber)
					|| FMath::RoundToInt(GroupNumber) != SelectedGroup->GroupId
					|| !Row->TryGetNumberField(TEXT("item"), ItemNumber))
				{
					continue;
				}
				FRokUnityEconomyItem Item;
				if (Economy && Economy->FindItemById(FMath::RoundToInt(ItemNumber), Item)
					&& Item.ShortcutPrice > 0)
				{
					DetailItems.Add(Item);
				}
			}
		}
	}
	const float DetailHeight = FMath::Max(547.0f, DetailItems.Num() * 156.0f);
	if (Page)
	{
		if (USizeBox* SizeBox = Page->Get<USizeBox>(TEXT("CityManagerDetailContentSize")))
		{
			SizeBox->SetHeightOverride(DetailHeight);
		}
	}
	else
	{
		DetailScroll->AddChild(WrapFixedSize(DetailContent, TEXT("CityManagerDetailContentSize"), FVector2D(1024.0f, DetailHeight)));
	}
	for (int32 Index = 0; Index < DetailItems.Num(); ++Index)
	{
		const FRokUnityEconomyItem& Item = DetailItems[Index];
		const float Y = Index * 156.0f;
		const TCHAR* IconAtlasPath = nullptr;
		FIntRect IconRect;
		FIntPoint IconAtlasSize;
		const bool bHasIcon = ResolveEconomyItemIcon(Item, IconAtlasPath, IconRect, IconAtlasSize);
		const FText ItemTitle = FText::FromString(Item.DisplayNameZhCn);
		const FText DescriptionText = FText::FromString(ResolveEconomyItemDescription(Item));
		if (URokUiPageWidget* Row = Page ? CreateWidget<URokUiPageWidget>(this, DetailClass) : nullptr)
		{
			Row->LegacyPrefix = TEXT("CityManagerDetail");
			Row->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
			Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			PlaceOnCanvas(DetailContent, Row, FVector2D(0.0f, Y), FVector2D(1024.0f, 156.0f), 1);
			UImage* Icon = Row->Get<UImage>(TEXT("Icon"));
			Row->SetPresent(Icon, bHasIcon);
			if (bHasIcon)
			{
				ApplyIcon(Icon, IconAtlasPath, IconRect, IconAtlasSize);
			}
			SetFeaturePageText(*Row, TEXT("ItemName"), ItemTitle);
			SetFeaturePageText(*Row, TEXT("ItemDescription"), DescriptionText);
			BindPageAction(*Row, TEXT("Action"), 2160 + Index);
			continue;
		}
		PlaceOnCanvas(DetailContent, MakeAtlasBorder(*FString::Printf(TEXT("CityManagerDetailRowBackground_%d"), Index), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(1024.0f, 154.0f), CommonPanelMargin), FVector2D(0.0f, Y), FVector2D(1024.0f, 154.0f), 0);
		if (bHasIcon)
		{
			PlaceOnCanvas(DetailContent, MakeAtlasImage(*FString::Printf(TEXT("CityManagerDetailIcon_%d"), Index), IconAtlasPath, IconRect, IconAtlasSize, FVector2D(112.0f)), FVector2D(884.0f, Y + 21.0f), FVector2D(112.0f), 3);
		}
		UTextBlock* ItemName = MakeScreenText(*FString::Printf(TEXT("CityManagerDetailItemName_%d"), Index), 22, FLinearColor(0.96f, 0.88f, 0.58f, 1.0f), true, ETextJustify::Right);
		ItemName->SetText(ItemTitle);
		PlaceOnCanvas(DetailContent, ItemName, FVector2D(410.0f, Y + 20.0f), FVector2D(450.0f, 35.0f), 3);
		UTextBlock* ItemDescription = MakeScreenText(*FString::Printf(TEXT("CityManagerDetailItemDescription_%d"), Index), 17, FLinearColor(0.82f, 0.82f, 0.78f, 1.0f), false, ETextJustify::Right);
		ItemDescription->SetAutoWrapText(true);
		ItemDescription->SetText(DescriptionText);
		PlaceOnCanvas(DetailContent, ItemDescription, FVector2D(330.0f, Y + 60.0f), FVector2D(530.0f, 65.0f), 3);
		PlaceOnCanvas(DetailContent, MakeActionButton(*FString::Printf(TEXT("CityManagerDetailAction_%d"), Index), 2160 + Index, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(188.0f, 54.0f), NSLOCTEXT("RokCityManager", "InventoryUnavailable", "库存未同步"), 16, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), FVector2D(50.0f, Y + 51.0f), FVector2D(188.0f, 54.0f), 7);
	}
	UE_LOG(LogTemp, Display, TEXT("Rok city manager: page=buff_detail group=%d config_items=%d authority=inventory_unsynced."), SelectedGroup->GroupId, DetailItems.Num());
}

namespace
{
TOptional<FRokServerArmySnapshot> MakeArmyShowPreviewSnapshot()
{
#if !UE_BUILD_SHIPPING
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly")))
	{
		return {};
	}
	FString State;
	FParse::Value(FCommandLine::Get(), TEXT("RokArmyShowFixture="), State);
	if (State != TEXT("waiting") && State != TEXT("empty") && State != TEXT("marches"))
	{
		return {};
	}

	FRokServerArmySnapshot Snapshot;
	Snapshot.Signature = State;
	Snapshot.bAvailable = true;
	Snapshot.bLoaded = Snapshot.bArmiesKnown = State != TEXT("waiting");
	if (State == TEXT("marches"))
	{
		FRokServerMarch& Active = Snapshot.Marches.AddDefaulted_GetRef();
		Active.ArmyIndex = 81;
		Active.Phase = ERokServerMarchPhase::Marching;
		Active.bTargetKnown = Active.bSoldiersKnown = Active.bTimeKnown = true;
		Active.TargetName = TEXT("本地行军演示");
		Active.Count = 640;
		Active.RemainingSeconds = 78;
		FRokServerMarch& Returning = Snapshot.Marches.AddDefaulted_GetRef();
		Returning.ArmyIndex = 83;
		Returning.Phase = ERokServerMarchPhase::Returning;
	}
	return Snapshot;
#else
	return {};
#endif
}

TOptional<FRokServerArmySnapshot> MakeArmyStatisticsPreviewSnapshot()
{
	const int32 Preview = ReadLocalUiPreviewOption(TEXT("RokArmyStatisticsFixture="), 1, 2);
	if (Preview == INDEX_NONE)
	{
		return {};
	}
	FRokServerArmySnapshot Snapshot;
	Snapshot.bAvailable = true;
	Snapshot.Signature = Preview == 1 ? TEXT("statistics_waiting") : TEXT("statistics_loaded");
	if (Preview == 1)
	{
		return Snapshot;
	}
	Snapshot.bLoaded = true;
	Snapshot.bCityKnown = true;
	Snapshot.bArmiesKnown = true;
	Snapshot.bTotalCountKnown = true;
	Snapshot.bCityPowerKnown = true;
	Snapshot.CityCount = 120;
	Snapshot.CityPower = 240;
	Snapshot.FieldCount = 30;
	Snapshot.FieldMinorCount = 5;
	Snapshot.TotalCount = 155;
	FRokServerSoldierStack& City = Snapshot.CitySoldiers.AddDefaulted_GetRef();
	City.Id = 1001;
	City.FamilyType = 1;
	City.Tier = 1;
	City.Count = 120;
	City.TotalPower = 240;
	City.Name = TEXT("持盾步兵");
	City.bCatalogKnown = true;
	FRokServerSoldierStack& Field = Snapshot.FieldSoldiers.AddDefaulted_GetRef();
	Field.Id = 4013;
	Field.FamilyType = 4;
	Field.Tier = 3;
	Field.Count = 30;
	Snapshot.AllSoldiers = {City, Field};
	return Snapshot;
}

FText FormatArmyShowMarchRemaining(const FRokServerMarch& March)
{
	if (!March.bTimeKnown)
	{
		return NSLOCTEXT("RokArmyShow", "MarchTimeUnknown", "时间待同步");
	}
	if (March.RemainingSeconds <= 0.0)
	{
		return NSLOCTEXT("RokArmyShow", "MarchAwaitingServer", "等待服务器确认");
	}
	return FText::Format(NSLOCTEXT("RokArmyShow", "MarchRemaining", "剩余 {0}"),
		FormatBuildDuration(FMath::CeilToInt(March.RemainingSeconds)));
}
}

void URokUnityFeatureScreenWidget::BuildArmyShow()
{
	const UGameInstance* ArmyInstance = GetGameInstance();
	const URokServerArmySubsystem* ServerArmy = ArmyInstance
		? ArmyInstance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const auto Preview = MakeArmyShowPreviewSnapshot();
	FRokServerArmySnapshot ArmySnapshot = Preview.IsSet() ? Preview.GetValue()
		: ServerArmy ? ServerArmy->GetSnapshot() : FRokServerArmySnapshot();
	if (!Preview.IsSet() && ArmySnapshot.bAvailable && !ArmySnapshot.bLoaded)
	{
		if (URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem())
		{
			if (!Catalog->IsCatalogLoaded()) Catalog->LoadCatalog();
			if (ServerArmy) ArmySnapshot = ServerArmy->GetSnapshot();
		}
	}
	bLastServerArmyAvailable = ArmySnapshot.bAvailable;
	LastServerArmySignature = ArmySnapshot.Signature;
	UClass* MarchClass = nullptr;
	UClass* MessageClass = nullptr;
	const URokUiPageWidget* Page = nullptr;
	if (bArmyShowRulesVisible)
	{
		Page = MountModePage(TEXT("ArmyShowRules"));
	}
	else
	{
		MarchClass = ResolveFeaturePageClass(TEXT("ArmyShowMarchRow"));
		MessageClass = ResolveFeaturePageClass(TEXT("ArmyShowMessage"));
		if (MarchClass && MessageClass)
		{
			Page = MountModePage();
		}
	}
	if (!Page)
	{
		BuildSplitWindowFrame(
			CapturePoint(133.5f, 85.5f),
			FVector2D(1173.0f, 729.0f),
			NSLOCTEXT("RokArmyShow", "Title", "部队一览"),
			CapturePoint(1230.0f, 77.0f));
	}

	if (bArmyShowRulesVisible)
	{
		if (Page)
		{
			BindPageAction(*Page, TEXT("ArmyShowRulesBackButton"), 2201);
			ArmyShowPanelAnimationWidgets.Add(Page->Get<UScrollBox>(TEXT("ArmyShowRulesScroll")));
			AdvanceArmyShowPanelAnimation(0.0f);
			return;
		}
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				TEXT("ArmyShowRulesBackButton"),
				2201,
				CommonAtlasPath,
				CommanderBackRect,
				CommonAtlasSize,
				FVector2D(69.0f)),
			CapturePoint(148.0f, 86.0f),
			FVector2D(69.0f),
			10);

		UScrollBox* RulesScroll = WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(),
			TEXT("ArmyShowRulesScroll"));
		RulesScroll->SetOrientation(Orient_Vertical);
		RulesScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		RulesScroll->SetAllowOverscroll(true);
		RulesScroll->SetAnimateWheelScrolling(true);
		RulesScroll->SetWheelScrollMultiplier(10.0f);
		PlaceOnCanvas(
			ModeCanvas,
			RulesScroll,
			CapturePoint(182.0f, 264.0f),
			FVector2D(1076.0f, 536.0f),
			4);

		UCanvasPanel* RulesContent = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("ArmyShowRulesContent"));
		RulesScroll->AddChild(WrapFixedSize(
			RulesContent,
			TEXT("ArmyShowRulesContentSize"),
			FVector2D(1076.0f, 535.0f)));
		UTextBlock* RulesText = MakeScreenText(
			TEXT("ArmyShowRulesText"),
			22,
			FLinearColor(0.85f, 0.83f, 0.73f, 1.0f));
		RulesText->SetAutoWrapText(true);
		RulesText->SetText(NSLOCTEXT(
			"RokArmyShow",
			"RulesDescription",
			"1. 长按城主城市并拖动至地图上的任意位置，便会弹出部队派遣界面，设置完毕后部队将会立即开始行军。\n"
			"2. 在古城和盟友城市都可以使用长按拖动的方式来快速移动自己的部队。\n"
			"3. 对野外部队使用长按拖动操作可快速移动这支部队。\n"
			"4. 在右侧的部队管理栏中长按拖动统帅头像至地图，也可快捷移动该统帅率领的部队。\n"
			"5. 双击一支己方部队，可以同时选中同屏的所有己方部队进行指挥。\n"
			"6. 自由行军的目标地点如果是不可行进区域，系统将自动使部队行军至最接近的位置。\n"
			"7. 溃退中的部队、集结部队和运输部队无法使用自由行军。\n"
			"8. 当使用长按拖动的方式向部队发出指令时，使用另一只手指轻触屏幕即可取消操作。"));
		PlaceOnCanvas(
			RulesContent,
			RulesText,
			FVector2D(30.0f, 0.0f),
			FVector2D(1016.0f, 523.0f),
			1);
		ArmyShowPanelAnimationWidgets.Add(RulesScroll);
		AdvanceArmyShowPanelAnimation(0.0f);
		return;
	}

	const int32 ArmyListFirstChild = ModeCanvas->GetChildrenCount();
	UScrollBox* ArmyList = nullptr;
	if (Page)
	{
		ArmyList = Page->Get<UScrollBox>(TEXT("ArmyShowListScroll"));
		BindPageAction(*Page, TEXT("ArmyShowRulesButton"), 2200);
		for (const FName Name : {FName(TEXT("ArmyShowRuleBar")), FName(TEXT("ArmyShowRuleLabel")),
			FName(TEXT("ArmyShowRulesButton")), FName(TEXT("ArmyShowRulesIcon")), FName(TEXT("ArmyShowListScroll"))})
		{
			ArmyShowPanelAnimationWidgets.Add(Page->Get<UWidget>(Name));
		}
	}
	else
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				TEXT("ArmyShowRuleBar"),
				CommonAtlasPath,
				ArmyShowRuleBarRect,
				CommonAtlasSize,
				FVector2D(986.0f, 48.0f)),
			CapturePoint(227.0f, 167.6f),
			FVector2D(986.0f, 48.0f),
			4);
		UTextBlock* RuleLabel = MakeScreenText(
			TEXT("ArmyShowRuleLabel"),
			24,
			FLinearColor(0.91f, 0.90f, 0.80f, 1.0f),
			false,
			ETextJustify::Center);
		RuleLabel->SetText(NSLOCTEXT(
			"RokArmyShow",
			"RuleLabel",
			"自由行军、部队多选和进攻驻扎的相关规则说明"));
		PlaceOnCanvas(
			ModeCanvas,
			RuleLabel,
			CapturePoint(308.3f, 167.0f),
			FVector2D(835.6f, 40.0f),
			6);
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				TEXT("ArmyShowRulesButton"),
				2200,
				nullptr,
				FIntRect(),
				FIntPoint::ZeroValue,
				FVector2D(60.0f)),
			CapturePoint(248.2f, 161.6f),
			FVector2D(60.0f),
			8);
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				TEXT("ArmyShowRulesIcon"),
				CommonAtlasPath,
				ArmyShowRuleIconRect,
				CommonAtlasSize,
				FVector2D(40.0f)),
			CapturePoint(258.2f, 171.6f),
			FVector2D(40.0f),
			9);

		ArmyList = WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(),
			TEXT("ArmyShowListScroll"));
		ArmyList->SetOrientation(Orient_Vertical);
		ArmyList->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		ArmyList->SetAllowOverscroll(true);
		ArmyList->SetAnimateWheelScrolling(true);
		ArmyList->SetWheelScrollMultiplier(10.0f);
		PlaceOnCanvas(
			ModeCanvas,
			ArmyList,
			CapturePoint(183.0f, 215.84f),
			FVector2D(1074.0f, 584.16f),
			4);
	}
	int32 RenderedRows = 0;
	if (ArmyList && ArmySnapshot.bAvailable)
	{
		if (!ArmySnapshot.bLoaded || !ArmySnapshot.bArmiesKnown || ArmySnapshot.Marches.IsEmpty())
		{
			const FText Message = ArmySnapshot.bLoaded && ArmySnapshot.bArmiesKnown
				? NSLOCTEXT("RokArmyShow", "NoActiveMarches", "暂无行军部队")
				: NSLOCTEXT("RokArmyShow", "MarchesWaiting", "等待服务器同步行军部队");
			URokUiPageWidget* MessageRow = Page ? CreateWidget<URokUiPageWidget>(this, MessageClass) : nullptr;
			if (MessageRow)
			{
				MessageRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				SetFeaturePageText(*MessageRow, TEXT("ArmyShowServerEmpty"), Message);
				ArmyList->AddChild(WrapFixedSize(MessageRow, TEXT("ArmyShowMessageSize"), FVector2D(1050.0f, 120.0f)));
			}
			else
			{
				UTextBlock* Empty = MakeScreenText(TEXT("ArmyShowServerEmpty"), 23,
					FLinearColor(0.84f, 0.84f, 0.80f, 1.0f), false, ETextJustify::Center);
				Empty->SetText(Message);
				ArmyList->AddChild(WrapFixedSize(Empty, TEXT("ArmyShowServerEmptySize"), FVector2D(1050.0f, 120.0f)));
			}
		}
		for (int32 Index = 0; ArmySnapshot.bLoaded && ArmySnapshot.bArmiesKnown
			&& Index < ArmySnapshot.Marches.Num(); ++Index)
		{
			const FRokServerMarch& March = ArmySnapshot.Marches[Index];
			const FText Phase = March.Phase == ERokServerMarchPhase::Marching
				? NSLOCTEXT("RokArmyShow", "Marching", "行军中")
				: March.Phase == ERokServerMarchPhase::Returning
					? NSLOCTEXT("RokArmyShow", "Returning", "返回中")
					: March.Phase == ERokServerMarchPhase::Garrisoned
						? NSLOCTEXT("RokArmyShow", "Garrisoned", "驻防中")
						: March.Phase == ERokServerMarchPhase::Collecting
							? NSLOCTEXT("RokArmyShow", "Collecting", "采集中")
							: March.Phase == ERokServerMarchPhase::Stationed
								? NSLOCTEXT("RokArmyShow", "Stationed", "驻扎中")
								: March.Phase == ERokServerMarchPhase::Battling
									? NSLOCTEXT("RokArmyShow", "Battling", "战斗中")
									: March.Phase == ERokServerMarchPhase::Rallying
										? NSLOCTEXT("RokArmyShow", "Rallying", "集结中")
										: NSLOCTEXT("RokArmyShow", "PhaseUnknown", "状态待同步");
			++RenderedRows;
			const FText StatusText = FText::Format(NSLOCTEXT("RokArmyShow", "MarchStatusFormat", "{0} · 兵力 {1}"),
				Phase, March.bSoldiersKnown ? FText::AsNumber(March.Count)
					: NSLOCTEXT("RokArmyShow", "MarchSoldiersUnknown", "待同步"));
			const FText TargetText = March.bTargetKnown
				? FText::FromString(March.TargetName.IsEmpty() ? TEXT("目标地点") : March.TargetName)
				: NSLOCTEXT("RokArmyShow", "TargetUnknown", "目标待同步");
			const FText RemainingText = FormatArmyShowMarchRemaining(March);
			const FText Tooltip = FText::AsNumber(March.ArmyIndex);
			if (URokUiPageWidget* MarchRow = Page ? CreateWidget<URokUiPageWidget>(this, MarchClass) : nullptr)
			{
				MarchRow->LegacyPrefix = TEXT("ArmyShowMarch");
				MarchRow->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
				MarchRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				if (UCanvasPanel* Row = MarchRow->Get<UCanvasPanel>(TEXT("Row")))
				{
					Row->SetToolTipText(Tooltip);
				}
				SetFeaturePageText(*MarchRow, TEXT("Status"), StatusText);
				SetFeaturePageText(*MarchRow, TEXT("Target"), TargetText);
				SetFeaturePageText(*MarchRow, TEXT("Remaining"), RemainingText);
				BindPageAction(*MarchRow, TEXT("Recall"), 6200 + Index);
				ArmyList->AddChild(WrapFixedSize(MarchRow,
					*FString::Printf(TEXT("ArmyShowMarchPresentationSize_%d"), Index), FVector2D(1050.0f, 124.0f)));
				continue;
			}

			UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
				*FString::Printf(TEXT("ArmyShowMarchRow_%d"), Index));
			Row->SetToolTipText(Tooltip);
			ArmyList->AddChild(WrapFixedSize(Row,
				*FString::Printf(TEXT("ArmyShowMarchRowSize_%d"), Index), FVector2D(1050.0f, 124.0f)));
			PlaceOnCanvas(Row, MakeAtlasBorder(*FString::Printf(TEXT("ArmyShowMarchBackground_%d"), Index),
				CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(1030.0f, 116.0f), CommonPanelMargin),
				FVector2D(8.0f, 2.0f), FVector2D(1030.0f, 116.0f), 0);
			UTextBlock* Status = MakeScreenText(*FString::Printf(TEXT("ArmyShowMarchStatus_%d"), Index), 23,
				FLinearColor(0.96f, 0.90f, 0.65f, 1.0f), true);
			Status->SetText(StatusText);
			PlaceOnCanvas(Row, Status, FVector2D(30.0f, 14.0f), FVector2D(550.0f, 36.0f), 1);
			UTextBlock* Target = MakeScreenText(*FString::Printf(TEXT("ArmyShowMarchTarget_%d"), Index), 18,
				FLinearColor::White);
			Target->SetText(TargetText);
			PlaceOnCanvas(Row, Target, FVector2D(30.0f, 60.0f), FVector2D(550.0f, 30.0f), 1);
			UTextBlock* Remaining = MakeScreenText(*FString::Printf(TEXT("ArmyShowMarchRemaining_%d"), Index), 18,
				FLinearColor(0.82f, 0.82f, 0.78f, 1.0f), false, ETextJustify::Right);
			Remaining->SetText(RemainingText);
			PlaceOnCanvas(Row, Remaining, FVector2D(650.0f, 17.0f), FVector2D(340.0f, 30.0f), 1);
			PlaceOnCanvas(Row, MakeActionButton(*FString::Printf(TEXT("ArmyShowMarchRecall_%d"), Index),
				6200 + Index, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize,
				FVector2D(160.0f, 45.0f), NSLOCTEXT("RokArmyShow", "Recall", "召回"),
				17, FLinearColor::White, ESlateBrushDrawType::Box, MiniBlueMargin),
				FVector2D(835.0f, 62.0f), FVector2D(160.0f, 45.0f), 3);
		}
	}

	// pl_blet and sv_armys share ContentShow; keep the frame/back button still.
	if (!Page)
	{
		for (int32 Index = ArmyListFirstChild; Index < ModeCanvas->GetChildrenCount(); ++Index)
		{
			ArmyShowPanelAnimationWidgets.Add(ModeCanvas->GetChildAt(Index));
		}
	}
	AdvanceArmyShowPanelAnimation(0.0f);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok ArmyShow screen: source=UI_Win_ArmyShow rendered_rows=%d placeholder_rows=0 authority=%s."),
		RenderedRows, Preview.IsSet() ? TEXT("fixture") : TEXT("server"));
}

void URokUnityFeatureScreenWidget::RefreshServerArmyPresentation()
{
	if (ActiveMode != ERokUnityFeatureScreenMode::ArmyStatistics
		&& ActiveMode != ERokUnityFeatureScreenMode::ArmyShow
		&& ActiveMode != ERokUnityFeatureScreenMode::TrainArmy) return;
	const UGameInstance* Instance = GetGameInstance();
	const URokServerArmySubsystem* Server = Instance
		? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	TOptional<FRokServerArmySnapshot> Preview;
	if (ActiveMode == ERokUnityFeatureScreenMode::ArmyShow)
	{
		Preview = MakeArmyShowPreviewSnapshot();
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::ArmyStatistics)
	{
		Preview = MakeArmyStatisticsPreviewSnapshot();
	}
	const FRokServerArmySnapshot Snapshot = Preview.IsSet() ? Preview.GetValue()
		: Server ? Server->GetSnapshot() : FRokServerArmySnapshot();
	if (Snapshot.bAvailable != bLastServerArmyAvailable || Snapshot.Signature != LastServerArmySignature)
	{
		const TCHAR* ScrollName = ActiveMode == ERokUnityFeatureScreenMode::ArmyStatistics
			? TEXT("ArmyStatisticsTroopList") : ActiveMode == ERokUnityFeatureScreenMode::ArmyShow
				? TEXT("ArmyShowListScroll") : nullptr;
		float Offset = 0.0f;
		if (ScrollName)
		{
			if (const UScrollBox* Scroll = Cast<UScrollBox>(GetWidgetFromName(ScrollName)))
				Offset = Scroll->GetScrollOffset();
		}
		RebuildModeContent();
		if (ScrollName)
		{
			if (UScrollBox* Scroll = Cast<UScrollBox>(GetWidgetFromName(ScrollName)))
				Scroll->SetScrollOffset(Offset);
		}
		return;
	}
	if (!Snapshot.bAvailable) return;
	if (ActiveMode == ERokUnityFeatureScreenMode::TrainArmy)
	{
		RefreshTrainArmyAction();
		if (UTextBlock* Status = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainArmyServerQueueStatus"))))
			Status->SetText(DescribeServerTrainingQueue(Snapshot,
				SelectedServerTrainBuildingIndex, SelectedTrainArmyType));
		if (UProgressBar* Progress = Cast<UProgressBar>(GetWidgetFromName(TEXT("TrainArmyServerQueueProgress"))))
		{
			const FRokServerTrainingQueue* Queue = Snapshot.bLoaded && Snapshot.bTrainingKnown
				&& FindServerTrainingBuilding(Snapshot, SelectedServerTrainBuildingIndex, SelectedTrainArmyType)
				? FindServerTrainingQueue(Snapshot, SelectedServerTrainBuildingIndex) : nullptr;
			Progress->SetPercent(Queue && Queue->bTimeKnown
				&& (Queue->State == ERokServerTrainingState::Training
					|| Queue->State == ERokServerTrainingState::Upgrading)
				? FMath::Clamp(static_cast<float>(Queue->Progress01), 0.0f, 1.0f) : 0.0f);
		}
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::ArmyShow && !bArmyShowRulesVisible)
	{
		for (int32 Index = 0; Index < Snapshot.Marches.Num(); ++Index)
		{
			UTextBlock* Remaining = Cast<UTextBlock>(GetWidgetFromName(
				*FString::Printf(TEXT("ArmyShowMarchRemaining_%d"), Index)));
			if (!Remaining) continue;
			const FRokServerMarch& March = Snapshot.Marches[Index];
			Remaining->SetText(FormatArmyShowMarchRemaining(March));
		}
	}
}

void URokUnityFeatureScreenWidget::AdvanceArmyShowPanelAnimation(float DeltaSeconds)
{
	ArmyShowPanelAnimationTime = FMath::Min(RokArmyShowPanelAnimation::Duration,
		ArmyShowPanelAnimationTime + FMath::Max(0.0f, DeltaSeconds));
	const auto Sample = RokArmyShowPanelAnimation::Evaluate(ArmyShowPanelAnimationTime, bArmyShowRulesVisible);
	for (const auto& WeakWidget : ArmyShowPanelAnimationWidgets)
	{
		if (UWidget* Widget = WeakWidget.Get())
		{
			Widget->SetRenderTranslation(FVector2D(Sample.X, 0));
			Widget->SetRenderOpacity(Sample.Opacity);
		}
	}
}

namespace
{
const FName SettingsHubButtonNames[] = {
	TEXT("SettingsHubCharacterButton"),
	TEXT("SettingsHubNotificationButton"),
	TEXT("SettingsHubGeneralButton"),
	TEXT("SettingsHubLanguageButton"),
	TEXT("SettingsHubCommunityButton"),
	TEXT("SettingsHubHelpButton"),
	TEXT("SettingsHubAccountButton")
};
constexpr int32 SettingsHubActionIds[] = {1455, 1454, 1453, 1450, 1458, 1457, 1456};
const FName LanguageOptionButtonNames[] = {
	TEXT("LanguageOptionEnglishButton"),
	TEXT("LanguageOptionArabicButton"),
	TEXT("LanguageOptionChineseButton"),
	TEXT("LanguageOptionTurkishButton")
};

FText FormatSettingsHubDate()
{
	return FText::Format(NSLOCTEXT("RokSettings", "DateFormat", "日期  {0}"),
		FText::FromString(FDateTime::Now().ToString(TEXT("%Y-%m-%d"))));
}

FText FormatSettingsHubVersion()
{
	return FText::Format(NSLOCTEXT("RokSettings", "VersionFormat", "版本  {0}"),
		FText::FromString(FString::Printf(TEXT("%s Preview"), FApp::GetProjectName())));
}
}

void URokUnityFeatureScreenWidget::BuildSettingsHub()
{
	if (const URokUiPageWidget* Page = MountModePage())
	{
		if (UTextBlock* Date = Page->Get<UTextBlock>(TEXT("SettingsHubDate")))
		{
			Date->SetText(FormatSettingsHubDate());
		}
		if (UTextBlock* Version = Page->Get<UTextBlock>(TEXT("SettingsHubVersion")))
		{
			Version->SetText(FormatSettingsHubVersion());
		}
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(SettingsHubButtonNames); ++Index)
		{
			BindPageAction(*Page, SettingsHubButtonNames[Index], SettingsHubActionIds[Index]);
		}
		UE_LOG(LogTemp, Log, TEXT("Rok settings hub: entries=7 active_routes=4 server_gates=1 platform_gates=2."));
		return;
	}
	BuildSplitWindowFrame(
		CapturePoint(146.0f, 73.0f),
		FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokSettings", "HubTitle", "设置"),
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("SettingsHubContentShade"),
			FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 136.0f),
		FVector2D(1088.0f, 664.0f),
		2);

	UTextBlock* DateText = MakeScreenText(
		TEXT("SettingsHubDate"),
		17,
		FLinearColor(0.78f, 0.78f, 0.75f, 1.0f),
		false,
		ETextJustify::Center);
	DateText->SetText(FormatSettingsHubDate());
	PlaceOnCanvas(
		ModeCanvas,
		DateText,
		CapturePoint(266.0f, 145.0f),
		FVector2D(396.0f, 40.0f),
		5);

	UTextBlock* VersionText = MakeScreenText(
		TEXT("SettingsHubVersion"),
		17,
		FLinearColor(0.78f, 0.78f, 0.75f, 1.0f),
		false,
		ETextJustify::Center);
	VersionText->SetText(FormatSettingsHubVersion());
	PlaceOnCanvas(
		ModeCanvas,
		VersionText,
		CapturePoint(778.0f, 145.0f),
		FVector2D(396.0f, 40.0f),
		5);

	static const FText Labels[] = {
		NSLOCTEXT("RokSettings", "Character", "角色管理"),
		NSLOCTEXT("RokSettings", "Notification", "通知"),
		NSLOCTEXT("RokSettings", "General", "通用设置"),
		NSLOCTEXT("RokSettings", "Language", "语言"),
		NSLOCTEXT("RokSettings", "Community", "社区"),
		NSLOCTEXT("RokSettings", "Help", "帮助与客服"),
		NSLOCTEXT("RokSettings", "Account", "账号")};
	static const FIntRect IconRects[] = {
		SettingCharacterIconRect,
		SettingNotificationIconRect,
		SettingGeneralIconRect,
		SettingLanguageIconRect,
		SettingCommunityIconRect,
		SettingHelpIconRect,
		SettingAccountIconRect};
	static const FVector2D Positions[] = {
		FVector2D(265.0f, 202.0f),
		FVector2D(529.0f, 202.0f),
		FVector2D(793.0f, 202.0f),
		FVector2D(1057.0f, 202.0f),
		FVector2D(529.0f, 402.0f),
		FVector2D(793.0f, 402.0f),
		FVector2D(1057.0f, 402.0f)};
	static const bool Enabled[] = {false, true, true, true, false, true, true};
	static const FText GateLabels[] = {
		NSLOCTEXT("RokSettings", "CharacterGate", "需服务器"),
		FText::GetEmpty(),
		FText::GetEmpty(),
		FText::GetEmpty(),
		NSLOCTEXT("RokSettings", "CommunityGate", "待平台接入"),
		NSLOCTEXT("RokSettings", "HelpGate", "待客服接入"),
		FText::GetEmpty()};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SettingsHubButtonNames); ++Index)
	{
		const FVector2D Position = Positions[Index];
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				*FString::Printf(TEXT("SettingsHubIcon_%d"), Index),
				SettingAtlasPath,
				IconRects[Index],
				SettingAtlasSize,
				FVector2D(126.0f)),
			CapturePoint(Position.X, Position.Y),
			FVector2D(126.0f),
			5);

		UTextBlock* Label = MakeScreenText(
			*FString::Printf(TEXT("SettingsHubLabel_%d"), Index),
			20,
			Enabled[Index]
				? FLinearColor::White
				: FLinearColor(0.68f, 0.68f, 0.65f, 1.0f),
			true,
			ETextJustify::Center);
		Label->SetText(Labels[Index]);
		PlaceOnCanvas(
			ModeCanvas,
			Label,
			CapturePoint(Position.X - 22.0f, Position.Y + 128.0f),
			FVector2D(170.0f, 34.0f),
			7);

		if (!GateLabels[Index].IsEmpty())
		{
			UTextBlock* Gate = MakeScreenText(
				*FString::Printf(TEXT("SettingsHubGateText_%d"), Index),
				14,
				FLinearColor(0.72f, 0.61f, 0.36f, 1.0f),
				false,
				ETextJustify::Center);
			Gate->SetText(GateLabels[Index]);
			PlaceOnCanvas(
				ModeCanvas,
				Gate,
				CapturePoint(Position.X - 22.0f, Position.Y + 158.0f),
				FVector2D(170.0f, 26.0f),
				7);
		}

		URokUnityIndexedButton* HitButton = MakeActionButton(
			SettingsHubButtonNames[Index],
			SettingsHubActionIds[Index],
			nullptr,
			FIntRect(),
			FIntPoint::ZeroValue,
			FVector2D(170.0f, 184.0f));
		HitButton->SetIsEnabled(Enabled[Index]);
		PlaceOnCanvas(
			ModeCanvas,
			HitButton,
			CapturePoint(Position.X - 22.0f, Position.Y - 8.0f),
			FVector2D(170.0f, 184.0f),
			9);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Rok settings hub: entries=7 active_routes=4 server_gates=1 platform_gates=2."));
}

namespace
{
void ApplyGraphicsSettingsRecommendation(URichTextBlock& Recommendation,
	const FTextBlockStyle* AuthoredDefault = nullptr, const FTextBlockStyle* AuthoredHighlight = nullptr)
{
	using RokGraphicsSettingsLayout::RecommendedTier;
	static const TCHAR* Levels[] = { TEXT("低"), TEXT("中"), TEXT("高") };
	FTextBlockStyle Style = Recommendation.GetDefaultTextStyle();
	// The transient data table colors only the recommended tier word.
	if (AuthoredDefault)
	{
		Style = *AuthoredDefault;
	}
	else
	{
		Style.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15))
			.SetColorAndOpacity(FSlateColor(FLinearColor::White)).SetShadowOffset(FVector2D::ZeroVector);
	}
	auto* Styles = NewObject<UDataTable>(&Recommendation);
	Styles->RowStruct = FRichTextStyleRow::StaticStruct();
	FRichTextStyleRow Default;
	Default.TextStyle = Style;
	Styles->AddRow(TEXT("Default"), Default);
	FRichTextStyleRow Green;
	Green.TextStyle = Style;
	if (AuthoredHighlight)
	{
		Green.TextStyle = *AuthoredHighlight;
	}
	else
	{
		Green.TextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(83.f / 255, 219.f / 255, 91.f / 255, 1)));
	}
	Styles->AddRow(TEXT("Recommended"), Green);
	Recommendation.SetTextStyleSet(Styles);
	Recommendation.SetDefaultTextStyle(Style);
	const int32 Tier = RecommendedTier(FPlatformMemory::GetConstants().TotalPhysical / (1024ull * 1024ull));
	Recommendation.SetText(FText::FromString(FString::Printf(
		TEXT("根据您的设备性能，我们推荐您选择图像质量:<Recommended>%s</>"), Levels[Tier - 1])));
}

TArray<FRokPersonalityToggleDefinition> GetVisiblePersonalityToggleDefinitions(const URokClientSettingsSubsystem* Settings)
{
	TArray<FRokPersonalityToggleDefinition> Definitions = Settings
		? Settings->GetPersonalityToggleDefinitions() : TArray<FRokPersonalityToggleDefinition>();
	Definitions.RemoveAll([](const FRokPersonalityToggleDefinition& Definition) { return !Definition.bVisibleInUnity; });
	Definitions.Sort([](const FRokPersonalityToggleDefinition& Left, const FRokPersonalityToggleDefinition& Right)
	{
		return Left.UnityDisplayOrder < Right.UnityDisplayOrder;
	});
	return Definitions;
}

FText FormatPersonalityToggleGate(const FRokPersonalityToggleDefinition& Definition)
{
	return Definition.UnityResetSeconds > 0
		? NSLOCTEXT("RokGeneralSettings", "ServerTimedGate", "由服务器时间控制，状态尚未同步")
		: NSLOCTEXT("RokGeneralSettings", "ConsumerGate", "相关功能尚未接入，当前不可更改");
}
}

void URokUnityFeatureScreenWidget::BuildGraphicsSettingsCard(UCanvasPanel* Content, float GraphicsValue, float FrameValue, bool bEnabled)
{
	using namespace RokGraphicsSettingsLayout;
	auto* Card = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GeneralSettingsGraphicsCard"));
	PlaceOnCanvas(Content, Card, FVector2D(37, 0), CardSize(), 3);
	auto Text = [this, Card](FName Name, const FText& Value, float Size, FLinearColor Color,
		FVector2D Position, FVector2D Extent, ETextJustify::Type Alignment = ETextJustify::Left)
	{
		auto* Label = MakeScreenText(Name, FMath::RoundToInt(Size), Color, false, Alignment);
		auto Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
		Label->SetText(Value); Label->SetShadowOffset(FVector2D::ZeroVector);
		auto* Region = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("%sRegion"), *Name.ToString()));
		Region->SetBrushColor(FLinearColor::Transparent); Region->SetPadding(FMargin(0)); Region->SetVerticalAlignment(VAlign_Center);
		Region->SetContent(Label); Region->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(Card, Region, Position, Extent, 4);
		return Label;
	};
	for (int32 Index = 0; Index < 3; ++Index)
		PlaceOnCanvas(Card, MakeAtlasBorder(*FString::Printf(TEXT("GeneralSettingsGraphicsLine_%d"), Index), CommonAtlasPath,
			SpriteRect(1, 611, 570, 2), CommonAtlasSize, FVector2D(960, 3), FMargin(220.f / 570, 0, 187.f / 570, 0)),
			FVector2D(20, Index == 0 ? 0 : Index == 1 ? 182.5 : 364), FVector2D(960, 3), 1);
	const FLinearColor TitleColor(226.f / 255, 209.f / 255, 155.f / 255, 1);
	Text(TEXT("GeneralSettingsGraphicsLabel"), FText::FromString(TEXT("图像质量")), 18, TitleColor, FVector2D(16, -7.55), FVector2D(505.8, 55.1));
	Text(TEXT("GeneralSettingsFrameRateLabel"), FText::FromString(TEXT("帧率")), 18, TitleColor, FVector2D(16, 198.45), FVector2D(505.8, 55.1));
	static const TCHAR* Levels[] = { TEXT("低"), TEXT("中"), TEXT("高") };
	auto* Recommendation = WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass(), TEXT("GeneralSettingsGraphicsRecommendation"));
	ApplyGraphicsSettingsRecommendation(*Recommendation);
	Recommendation->SetAutoWrapText(true); Recommendation->SetWrapTextAt(524.05f); Recommendation->SetJustification(ETextJustify::Left);
	auto* RecommendationRegion = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GeneralSettingsGraphicsRecommendationRegion"));
	RecommendationRegion->SetBrushColor(FLinearColor::Transparent); RecommendationRegion->SetPadding(FMargin(0));
	RecommendationRegion->SetVerticalAlignment(VAlign_Center); RecommendationRegion->SetContent(Recommendation);
	RecommendationRegion->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(Card, RecommendationRegion, FVector2D(16, 30.45), FVector2D(524.05, 55.1), 4);
	for (int32 Row = 0; Row < 2; ++Row)
	{
		const FString Prefix = Row == 0 ? TEXT("GeneralSettingsGraphics") : TEXT("GeneralSettingsFrameRate");
		const int32 Level = Tier(Row == 0 ? GraphicsValue : FrameValue);
		PlaceOnCanvas(Card, MakeAtlasImage(*(Prefix + TEXT("Plate")), SettingAtlasPath, SettingSliderPlateRect, SettingAtlasSize, FVector2D(883, 53)),
			FVector2D(58.5, 83.5 + Row * 182), FVector2D(883, 53), 2);
		PlaceOnCanvas(Card, MakeAtlasBorder(*(Prefix + TEXT("Background")), CommonAtlasPath, SpriteRect(401, 112, 20, 23), CommonAtlasSize,
			FVector2D(824, 22), FMargin(.5, 7.f / 23, .5, 6.f / 23)), FVector2D(88, 99 + Row * 182), FVector2D(824, 22), 3);
		auto* Fill = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *(Prefix + TEXT("Fill")));
		Fill->SetClipping(EWidgetClipping::ClipToBoundsAlways); Fill->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(Card, Fill, FillPosition(Row), FillSize(Level), 4);
		for (int32 Patch = 0; Patch < 3; ++Patch)
		{
			const int32 X[] = { 384, 389, 393 }, W[] = { 5, 4, 3 };
			const float Width = FillSize(Level).X;
			const float DestX = Patch == 0 ? 0.f : Patch == 1 ? 5.f : FMath::Max(5.f, Width - 3.f);
			const float DestW = Patch == 1 ? FMath::Max(0.f, Width - 8.f) : static_cast<float>(W[Patch]);
			PlaceOnCanvas(Fill, MakeAtlasImage(*FString::Printf(TEXT("%sFillPatch_%d"), *Prefix, Patch), CommonAtlasPath,
				SpriteRect(X[Patch], 59, W[Patch], 19), CommonAtlasSize, FVector2D(DestW, 17.4)), FVector2D(DestX, 0), FVector2D(DestW, 17.4), 0);
		}
		auto* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), *(Prefix + TEXT("Slider")));
		Slider->SetMinValue(1); Slider->SetMaxValue(3); Slider->SetStepSize(1); Slider->MouseUsesStep = true;
		Slider->SetIndentHandle(false); Slider->SetValue(Level); Slider->SetIsEnabled(bEnabled);
		FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		FSliderStyle SliderStyle; SliderStyle.SetNormalBarImage(Empty).SetHoveredBarImage(Empty).SetDisabledBarImage(Empty);
		const auto Thumb = MakeAtlasBrush(LoadUiTexture(CommonAtlasPath), SpriteRect(679, 474, 20, 40), CommonAtlasSize, FVector2D(22, 41));
		SliderStyle.SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDisabledThumbImage(Thumb); Slider->SetWidgetStyle(SliderStyle);
		PlaceOnCanvas(Card, Slider, SliderPosition(Row), SliderSize(), 5);
		if (bEnabled)
		{
			if (Row == 0) Slider->OnValueChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleGraphicsQualitySliderChanged);
			else Slider->OnValueChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleFrameRateSliderChanged);
		}
		for (int32 Index = 0; Index < 3; ++Index)
			Text(*FString::Printf(TEXT("%sScale_%d"), *Prefix, Index), FText::FromString(Levels[Index]), 15, FLinearColor::White,
				FVector2D(3.945 + Index * 436, 128.75 + Row * 182 + (Row == 0 && Index == 1 ? 1 : 0)), FVector2D(120.11, 55.1), ETextJustify::Center);
		// Keep existing diagnostic value identities; the source card only displays the three scale labels.
		auto* LegacyValue = Text(*(Prefix + TEXT("Value")), FText::FromString(Row == 0 ? Levels[Level - 1] : Level == 3 ? TEXT("60 FPS") : TEXT("30 FPS")),
			15, FLinearColor::White, FVector2D::ZeroVector, FVector2D(1));
		LegacyValue->SetVisibility(ESlateVisibility::Collapsed);
		if (Row == 0) GeneralSettingsGraphicsValueText = LegacyValue; else GeneralSettingsFrameRateValueText = LegacyValue;
	}
}

void URokUnityFeatureScreenWidget::UpdateGraphicsSettingsSlider(const FName Prefix, int32 Level)
{
	if (auto* Slider = Cast<USlider>(GetWidgetFromName(*(Prefix.ToString() + TEXT("Slider"))))) Slider->SetValue(Level);
	if (auto* Fill = GetWidgetFromName(*(Prefix.ToString() + TEXT("Fill"))))
		if (auto* FillSlot = Cast<UCanvasPanelSlot>(Fill->Slot)) FillSlot->SetSize(RokGraphicsSettingsLayout::FillSize(Level));
	const float Width = RokGraphicsSettingsLayout::FillSize(Level).X;
	for (int32 Patch = 0; Patch < 3; ++Patch)
		if (auto* Widget = GetWidgetFromName(*FString::Printf(TEXT("%sFillPatch_%d"), *Prefix.ToString(), Patch)))
			if (auto* FillSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
			{
				FillSlot->SetPosition(FVector2D(Patch == 0 ? 0.f : Patch == 1 ? 5.f : FMath::Max(5.f, Width - 3.f), 0));
				FillSlot->SetSize(FVector2D(Patch == 0 ? 5.f : Patch == 1 ? FMath::Max(0.f, Width - 8.f) : 3.f, 17.4));
			}
}

void URokUnityFeatureScreenWidget::BindGeneralSettingsPage(const URokUiPageWidget& Page)
{
	using namespace RokGraphicsSettingsLayout;
	URokClientSettingsSubsystem* Settings = GetClientSettingsSubsystem();
	auto InitializeGraphicsSlider = [this, &Page, Settings](const FString& Prefix, float Value)
	{
		if (USlider* Slider = Page.Get<USlider>(*(Prefix + TEXT("Slider"))))
		{
			Slider->SetIsEnabled(Settings != nullptr);
			UpdateGraphicsSettingsSlider(*Prefix, Tier(Value));
		}
	};
	InitializeGraphicsSlider(TEXT("GeneralSettingsGraphics"),
		Settings ? static_cast<float>(Settings->GetGraphicsQualityTier()) : 3.0f);
	InitializeGraphicsSlider(TEXT("GeneralSettingsFrameRate"),
		Settings ? static_cast<float>(Settings->GetFrameRateOption()) : 3.0f);
	if (Settings)
	{
		if (USlider* Slider = Page.Get<USlider>(TEXT("GeneralSettingsGraphicsSlider")))
		{
			Slider->OnValueChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleGraphicsQualitySliderChanged);
		}
		if (USlider* Slider = Page.Get<USlider>(TEXT("GeneralSettingsFrameRateSlider")))
		{
			Slider->OnValueChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleFrameRateSliderChanged);
		}
	}
	GeneralSettingsGraphicsValueText = Page.Get<UTextBlock>(TEXT("GeneralSettingsGraphicsValue"));
	GeneralSettingsFrameRateValueText = Page.Get<UTextBlock>(TEXT("GeneralSettingsFrameRateValue"));
	if (GeneralSettingsGraphicsValueText)
	{
		static const TCHAR* Labels[] = { TEXT("低"), TEXT("中"), TEXT("高") };
		const int32 Tier = Settings ? static_cast<int32>(Settings->GetGraphicsQualityTier()) : 3;
		GeneralSettingsGraphicsValueText->SetText(FText::FromString(Labels[RokGraphicsSettingsLayout::Tier(Tier) - 1]));
	}
	if (GeneralSettingsFrameRateValueText)
	{
		const int32 Option = Settings ? static_cast<int32>(Settings->GetFrameRateOption()) : 3;
		GeneralSettingsFrameRateValueText->SetText(FText::FromString(Option == 3 ? TEXT("60 FPS") : TEXT("30 FPS")));
	}
	if (URichTextBlock* Recommendation = Page.Get<URichTextBlock>(TEXT("GeneralSettingsGraphicsRecommendation")))
	{
		FTextBlockStyle DefaultStyle, HighlightStyle;
		const bool bAuthored = TryGetFeatureTextStyle(TEXT("GeneralSettingsRecommendationDefault"), DefaultStyle)
			&& TryGetFeatureTextStyle(TEXT("GeneralSettingsRecommendationHighlight"), HighlightStyle);
		ApplyGraphicsSettingsRecommendation(*Recommendation, bAuthored ? &DefaultStyle : nullptr,
			bAuthored ? &HighlightStyle : nullptr);
	}
	if (USlider* SfxSlider = Page.Get<USlider>(TEXT("GeneralSettingsSfxSlider")))
	{
		const float Volume = Settings ? Settings->GetSfxVolume() : 1.0f;
		SfxSlider->SetValue(Volume);
		UpdateGeneralSettingsAudioFill(TEXT("GeneralSettingsSfx"), Volume);
		SfxSlider->OnValueChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleSfxVolumeSliderChanged);
	}
	if (USlider* MusicSlider = Page.Get<USlider>(TEXT("GeneralSettingsMusicSlider")))
	{
		const float Volume = Settings ? Settings->GetMusicVolume() : 1.0f;
		MusicSlider->SetValue(Volume);
		UpdateGeneralSettingsAudioFill(TEXT("GeneralSettingsMusic"), Volume);
		MusicSlider->OnValueChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleMusicVolumeSliderChanged);
	}

	const TArray<FRokPersonalityToggleDefinition> Definitions = GetVisiblePersonalityToggleDefinitions(Settings);
	VisiblePersonalityToggleNames.Reset();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FName Row(*FString::Printf(TEXT("GeneralSettingsPersonalityRow_%d"), Index));
		const FName Label(*FString::Printf(TEXT("GeneralSettingsPersonalityLabel_%d"), Index));
		const FName Gate(*FString::Printf(TEXT("GeneralSettingsPersonalityGate_%d"), Index));
		const FName Toggle(*FString::Printf(TEXT("GeneralSettingsPersonality_%d"), Index));
		const FName Knob(*FString::Printf(TEXT("GeneralSettingsPersonalityKnob_%d"), Index));
		if (!Definitions.IsValidIndex(Index))
		{
			for (const FName Name : {Row, Label, Gate, Toggle, Knob})
			{
				Page.SetPresent(Page.Get<UWidget>(Name), false);
			}
			continue;
		}

		const FRokPersonalityToggleDefinition& Definition = Definitions[Index];
		VisiblePersonalityToggleNames.Add(Definition.Name);
		if (UTextBlock* LabelText = Page.Get<UTextBlock>(Label))
		{
			LabelText->SetText(ResolvePersonalityToggleLabel(Definition.Name));
		}
		if (UTextBlock* GateText = Page.Get<UTextBlock>(Gate))
		{
			GateText->SetText(FormatPersonalityToggleGate(Definition));
		}
		BindPageAction(Page, Toggle, 1540 + Index);
		const bool bEnabled = Settings->GetPersonalityToggle(Definition.Name);
		if (bEnabled == Definition.bDefaultEnabled)
		{
			continue;
		}
		if (UImage* Background = Page.Get<UImage>(*(Toggle.ToString() + TEXT("Background"))))
		{
			FSlateBrush Brush = Background->GetBrush();
			const FSlateBrush StateBrush = MakeAtlasBrush(LoadUiTexture(CommonAtlasPath),
				bEnabled ? SettingSwitchOnRect : SettingSwitchOffRect, CommonAtlasSize, Brush.ImageSize);
			Brush.SetUVRegion(static_cast<FBox2f>(StateBrush.GetUVRegion()));
			Background->SetBrush(Brush);
		}
		if (UWidget* KnobImage = Page.Get<UWidget>(Knob))
		{
			FWidgetTransform Transform = KnobImage->GetRenderTransform();
			Transform.Translation.X += bEnabled ? 37.0f : -37.0f;
			KnobImage->SetRenderTransform(Transform);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("Rok general settings: video=applied audio=router_enabled personality=unconsumed_disabled rows=%d."),
		VisiblePersonalityToggleNames.Num());
}

void URokUnityFeatureScreenWidget::BuildGeneralSettings()
{
	if (const URokUiPageWidget* Page = MountModePage())
	{
		BindGeneralSettingsPage(*Page);
		return;
	}
	BuildSplitWindowFrame(
		CapturePoint(146.0f, 73.0f),
		FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokGeneralSettings", "Title", "通用设置"),
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("GeneralSettingsContentShade"),
			FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 136.0f),
		FVector2D(1088.0f, 664.0f),
		2);

	URokClientSettingsSubsystem* Settings = GetClientSettingsSubsystem();
	const float GraphicsValue = Settings
		? static_cast<float>(Settings->GetGraphicsQualityTier())
		: 3.0f;
	const float FrameRateValue = Settings
		? static_cast<float>(Settings->GetFrameRateOption())
		: 3.0f;
	const float MusicVolume = Settings ? Settings->GetMusicVolume() : 1.0f;
	const float SfxVolume = Settings ? Settings->GetSfxVolume() : 1.0f;

	UScrollBox* SettingsScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("GeneralSettingsScroll"));
	SettingsScroll->SetOrientation(Orient_Vertical);
	SettingsScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	// UI_Win_GeneralSetting.rect is 1074 x 664. Its stretched
	// sv_scroll_view has sizeDelta (-69.6, -33.85) and anchored
	// position (+2.8, -16.925) in the 1440 x 810 source canvas.
	PlaceOnCanvas(
		ModeCanvas,
		SettingsScroll,
		FVector2D(220.6f, 124.85f),
		FVector2D(1004.4f, 630.15f),
		4);
	SettingsScroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);

	UCanvasPanel* SettingsContent = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("GeneralSettingsScrollContent"));
	SettingsScroll->AddChild(WrapFixedSize(
		SettingsContent,
		TEXT("GeneralSettingsScrollContentSize"),
		FVector2D(1004.4f, 1299.0f)));
	// Keep the existing graphics/personality child coordinates intact while
	// moving their 37-pixel wrapper inset to the source scroll viewport.
	UCanvasPanel* ExistingRows = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("GeneralSettingsExistingRows"));
	PlaceOnCanvas(SettingsContent, ExistingRows, FVector2D(-37.0f, 0.0f),
		FVector2D(1074.0f, 1299.0f), 0);

	BuildGraphicsSettingsCard(ExistingRows, GraphicsValue, FrameRateValue, Settings != nullptr);

	// UI_Item_MusicSetting is the second zero-spacing ListView item: 1000 x
	// 367, SFX above music. The four endpoint images are non-raycast art,
	// not mute buttons. Slider value zero is the source mute state.
	UCanvasPanel* AudioItem = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("GeneralSettingsAudioItem"));
	PlaceOnCanvas(SettingsContent, AudioItem, FVector2D(0.0f, 367.0f),
		FVector2D(1000.0f, 367.0f), 1);
	for (int32 LineIndex = 0; LineIndex < 2; ++LineIndex)
	{
		PlaceOnCanvas(AudioItem,
			MakeAtlasBorder(*FString::Printf(TEXT("GeneralSettingsAudioLine_%d"), LineIndex),
				CommonAtlasPath, SpriteRect(1, 611, 570, 2), CommonAtlasSize,
				FVector2D(960.0f, 3.0f), FMargin(220.0f / 570.0f, 0.0f, 187.0f / 570.0f, 0.0f)),
			FVector2D(20.0f, LineIndex == 0 ? 0.0f : 364.0f), FVector2D(960.0f, 3.0f), 1);
	}
	const auto AddAudioText = [this, AudioItem](const FName Name, const FText& Text,
		const float FontSize, const FLinearColor Color, const FVector2D Position,
		const FVector2D Size)
	{
		UTextBlock* Label = MakeScreenText(Name, FMath::RoundToInt(FontSize), Color,
			false, ETextJustify::Right);
		FSlateFontInfo Font = Label->GetFont();
		Font.Size = FontSize;
		Label->SetFont(Font);
		Label->SetText(Text);
		Label->SetShadowOffset(FVector2D::ZeroVector);
		UBorder* Region = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			*FString::Printf(TEXT("%sRegion"), *Name.ToString()));
		Region->SetBrushColor(FLinearColor::Transparent);
		Region->SetPadding(FMargin(0.0f));
		Region->SetHorizontalAlignment(HAlign_Fill);
		Region->SetVerticalAlignment(VAlign_Center);
		Region->SetContent(Label);
		Region->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(AudioItem, Region, Position, Size, 4);
	};
	AddAudioText(TEXT("GeneralSettingsSfxTitle"), FText::FromString(TEXT("音效音量")),
		18.0f, FLinearColor::FromSRGBColor(FColor(226, 209, 155)),
		FVector2D(478.2f, -6.55f), FVector2D(505.8f, 55.1f));
	AddAudioText(TEXT("GeneralSettingsSfxDescription"),
		FText::FromString(TEXT("调整游戏内音效的音量大小")), 15.0f,
		FLinearColor::White, FVector2D(-24.0f, 30.55f), FVector2D(1008.0f, 55.1f));
	AddAudioText(TEXT("GeneralSettingsMusicTitle"), FText::FromString(TEXT("音乐音量")),
		18.0f, FLinearColor::FromSRGBColor(FColor(226, 209, 155)),
		FVector2D(478.2f, 147.15f), FVector2D(505.8f, 55.1f));
	AddAudioText(TEXT("GeneralSettingsMusicDescription"),
		FText::FromString(TEXT("调整游戏内背景音乐的音量大小")), 15.0f,
		FLinearColor::White, FVector2D(-24.0f, 184.85f), FVector2D(1008.0f, 55.1f));

	const auto AddAudioRow = [this, AudioItem](const FName Prefix,
		const float CenterY, const float Volume) -> USlider*
	{
		const FString Base = Prefix.ToString();
		PlaceOnCanvas(AudioItem,
			MakeAtlasBorder(*(Base + TEXT("Frame")), CommonAtlasPath,
				CommonPanelRect, CommonAtlasSize, FVector2D(850.0f, 42.0f), CommonPanelMargin),
			FVector2D(75.0f, CenterY - 21.0f), FVector2D(850.0f, 42.0f), 2);
		PlaceOnCanvas(AudioItem,
			MakeAtlasBorder(*(Base + TEXT("Background")), CommonAtlasPath,
				SpriteRect(401, 112, 20, 23), CommonAtlasSize, FVector2D(822.0f, 22.0f),
				FMargin(0.5f, 7.0f / 23.0f, 0.5f, 6.0f / 23.0f)),
			FVector2D(89.0f, CenterY - 11.0f), FVector2D(822.0f, 22.0f), 3);
		UCanvasPanel* Fill = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), *(Base + TEXT("Fill")));
		Fill->SetClipping(EWidgetClipping::ClipToBoundsAlways);
		Fill->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(AudioItem, Fill, FVector2D(92.895f + 818.55f *
			(1.0f - FMath::Clamp(Volume, 0.0f, 1.0f)), CenterY - 9.5f),
			FVector2D(818.55f * FMath::Clamp(Volume, 0.0f, 1.0f), 19.0f), 4);
		for (int32 Patch = 0; Patch < 3; ++Patch)
		{
			const int32 SourceX[] = { 384, 389, 393 };
			const int32 SourceW[] = { 5, 4, 3 };
			const float Width = 818.55f * FMath::Clamp(Volume, 0.0f, 1.0f);
			const float X = Patch == 0 ? 0.0f : Patch == 1 ? 5.0f : FMath::Max(5.0f, Width - 3.0f);
			const float W = Patch == 1 ? FMath::Max(0.0f, Width - 8.0f) : SourceW[Patch];
			PlaceOnCanvas(Fill,
				MakeAtlasImage(*FString::Printf(TEXT("%sFillPatch_%d"), *Base, Patch),
					CommonAtlasPath, SpriteRect(SourceX[Patch], 59, SourceW[Patch], 19),
					CommonAtlasSize, FVector2D(W, 19.0f)),
				FVector2D(X, 0.0f), FVector2D(W, 19.0f), 0);
		}
		USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(),
			*(Base + TEXT("Slider")));
		Slider->SetMinValue(0.0f);
		Slider->SetMaxValue(1.0f);
		Slider->SetStepSize(0.001f);
		Slider->MouseUsesStep = false;
		Slider->SetIndentHandle(false);
		Slider->SetValue(FMath::Clamp(Volume, 0.0f, 1.0f));
		FSlateBrush Empty;
		Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		FSliderStyle Style;
		Style.SetNormalBarImage(Empty).SetHoveredBarImage(Empty).SetDisabledBarImage(Empty);
		const FSlateBrush Thumb = MakeAtlasBrush(LoadUiTexture(CommonAtlasPath),
			SpriteRect(679, 474, 20, 40), CommonAtlasSize, FVector2D(22.0f, 41.0f));
		Style.SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDisabledThumbImage(Thumb);
		Slider->SetWidgetStyle(Style);
		Slider->SetIsEnabled(true);
		// Unity Slider.m_Direction=1 is RightToLeft. Reflect only the
		// interactive track; its source thumb artwork is horizontally symmetric.
		Slider->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Slider->SetRenderScale(FVector2D(-1.0f, 1.0f));
		PlaceOnCanvas(AudioItem, Slider, FVector2D(90.895f, CenterY - 11.005f),
			FVector2D(822.55f, 22.01f), 5);
		return Slider;
	};
	USlider* SfxSlider = AddAudioRow(TEXT("GeneralSettingsSfx"), 109.64f, SfxVolume);
	USlider* MusicSlider = AddAudioRow(TEXT("GeneralSettingsMusic"), 264.0f, MusicVolume);
	// Source endpoint sprites have RaycastTarget=0; no independent mute action.
	PlaceOnCanvas(AudioItem, MakeAtlasImage(TEXT("GeneralSettingsSfxIconLeft"),
		SettingAtlasPath, SettingMusicIconRect, SettingAtlasSize, FVector2D(46.0f, 45.0f)),
		FVector2D(0.3f, 86.2f), FVector2D(46.0f, 45.0f), 6);
	PlaceOnCanvas(AudioItem, MakeAtlasImage(TEXT("GeneralSettingsSfxIconRight"),
		SettingAtlasPath, SettingSfxIconRect, SettingAtlasSize, FVector2D(41.0f, 45.0f)),
		FVector2D(959.5f, 88.0f), FVector2D(41.0f, 45.0f), 6);
	PlaceOnCanvas(AudioItem, MakeAtlasImage(TEXT("GeneralSettingsMusicIconLeft"),
		SettingAtlasPath, SettingMusicIconRect, SettingAtlasSize, FVector2D(46.0f, 45.0f)),
		FVector2D(-0.1f, 240.7f), FVector2D(46.0f, 45.0f), 6);
	PlaceOnCanvas(AudioItem, MakeAtlasImage(TEXT("GeneralSettingsMusicIconRight"),
		SettingAtlasPath, SettingSfxIconRect, SettingAtlasSize, FVector2D(41.0f, 45.0f)),
		FVector2D(958.9f, 242.4f), FVector2D(41.0f, 45.0f), 6);
	SfxSlider->OnValueChanged.AddDynamic(this,
		&URokUnityFeatureScreenWidget::HandleSfxVolumeSliderChanged);
	MusicSlider->OnValueChanged.AddDynamic(this,
		&URokUnityFeatureScreenWidget::HandleMusicVolumeSliderChanged);

	VisiblePersonalityToggleNames.Reset();
	const TArray<FRokPersonalityToggleDefinition> Definitions = GetVisiblePersonalityToggleDefinitions(Settings);
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		const FRokPersonalityToggleDefinition* Definition = &Definitions[Index];
		VisiblePersonalityToggleNames.Add(Definition->Name);
		const float RowY = 734.0f + Index * 94.1f;
		const bool bEnabled = Settings
			&& Settings->GetPersonalityToggle(Definition->Name);
		PlaceOnCanvas(
			ExistingRows,
			MakeAtlasBorder(
				*FString::Printf(TEXT("GeneralSettingsPersonalityRow_%d"), Index),
				CommonAtlasPath,
				CommonPanelRect,
				CommonAtlasSize,
				FVector2D(1000.0f, 94.1f),
				CommonPanelMargin),
			FVector2D(37.0f, RowY),
			FVector2D(1000.0f, 94.1f),
			1);
		UTextBlock* Label = MakeScreenText(
			*FString::Printf(TEXT("GeneralSettingsPersonalityLabel_%d"), Index),
			19,
			FLinearColor::White,
			true);
		Label->SetText(ResolvePersonalityToggleLabel(Definition->Name));
		PlaceOnCanvas(
			ExistingRows,
			Label,
			FVector2D(77.0f, RowY + 13.0f),
			FVector2D(640.0f, 34.0f),
			3);
		UTextBlock* GateText = MakeScreenText(
			*FString::Printf(TEXT("GeneralSettingsPersonalityGate_%d"), Index),
			14,
			FLinearColor(0.70f, 0.66f, 0.54f, 1.0f));
		GateText->SetText(FormatPersonalityToggleGate(*Definition));
		PlaceOnCanvas(
			ExistingRows,
			GateText,
			FVector2D(77.0f, RowY + 49.0f),
			FVector2D(690.0f, 28.0f),
			3);

		URokUnityIndexedButton* ToggleButton = MakeActionButton(
			*FString::Printf(TEXT("GeneralSettingsPersonality_%d"), Index),
			1540 + Index,
			CommonAtlasPath,
			bEnabled ? SettingSwitchOnRect : SettingSwitchOffRect,
			CommonAtlasSize,
			FVector2D(82.0f, 40.0f));
		ToggleButton->SetIsEnabled(false);
		PlaceOnCanvas(
			ExistingRows,
			ToggleButton,
			FVector2D(900.0f, RowY + 26.0f),
			FVector2D(82.0f, 40.0f),
			4);
		PlaceOnCanvas(
			ExistingRows,
			MakeAtlasImage(
				*FString::Printf(TEXT("GeneralSettingsPersonalityKnob_%d"), Index),
				CommonAtlasPath,
				SettingSwitchKnobRect,
				CommonAtlasSize,
				FVector2D(43.0f)),
			FVector2D(
				bEnabled ? 936.0f : 899.0f,
				RowY + 24.0f),
			FVector2D(43.0f),
			5);
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Rok general settings: video=applied audio=router_enabled personality=unconsumed_disabled rows=%d."),
		VisiblePersonalityToggleNames.Num());
}

void URokUnityFeatureScreenWidget::BuildNotificationSettings()
{
	if (const URokUiPageWidget* Page = MountModePage())
	{
		for (int32 Index = 0; Index < 10; ++Index)
		{
			BindPageAction(*Page, *FString::Printf(TEXT("NotificationSetting_%d"), Index + 1), 1470 + Index);
		}
		UE_LOG(LogTemp, Log, TEXT("Rok notification settings: rows=10 state=unknown authority=server optimistic_update=0."));
		return;
	}
	BuildSplitWindowFrame(
		CapturePoint(133.5f, 85.5f),
		FVector2D(1173.0f, 729.0f),
		NSLOCTEXT("RokNotificationSettings", "Title", "通知设置"),
		CapturePoint(1230.0f, 89.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("NotificationSettingsContentShade"),
			FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(183.0f, 136.0f),
		FVector2D(1074.0f, 664.0f),
		2);

	UTextBlock* StateText = MakeScreenText(
		TEXT("NotificationSettingsServerState"),
		17,
		FLinearColor(0.86f, 0.72f, 0.42f, 1.0f),
		false,
		ETextJustify::Center);
	StateText->SetText(NSLOCTEXT(
		"RokNotificationSettings",
		"ServerState",
		"角色推送设置尚未从服务器同步，以下项目不可更改。"));
	PlaceOnCanvas(
		ModeCanvas,
		StateText,
		CapturePoint(250.0f, 150.0f),
		FVector2D(940.0f, 36.0f),
		5);

	UScrollBox* List = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("NotificationSettingsScroll"));
	List->SetOrientation(Orient_Vertical);
	List->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(
		ModeCanvas,
		List,
		CapturePoint(210.0f, 190.0f),
		FVector2D(1019.0f, 570.0f),
		4);

	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("NotificationSettingsScrollContent"));
	List->AddChild(WrapFixedSize(
		Content,
		TEXT("NotificationSettingsScrollContentSize"),
		FVector2D(1019.0f, 1000.0f)));

	static const FText NotificationLabels[] = {
		NSLOCTEXT("RokNotificationSettings", "UpgradeTraining", "升级与训练"),
		NSLOCTEXT("RokNotificationSettings", "Battle", "战斗通知"),
		NSLOCTEXT("RokNotificationSettings", "PersonalMail", "个人邮件"),
		NSLOCTEXT("RokNotificationSettings", "AllianceMail", "联盟邮件"),
		NSLOCTEXT("RokNotificationSettings", "Activity", "活动开启"),
		NSLOCTEXT("RokNotificationSettings", "CollectResources", "收取资源"),
		NSLOCTEXT("RokNotificationSettings", "AllianceChat", "联盟聊天"),
		NSLOCTEXT("RokNotificationSettings", "TroopReturn", "部队返回"),
		NSLOCTEXT("RokNotificationSettings", "DailySupply", "每日补给"),
		NSLOCTEXT("RokNotificationSettings", "AllianceRally", "联盟集结")};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NotificationLabels); ++Index)
	{
		const float Y = Index * 100.0f;
		PlaceOnCanvas(
			Content,
			MakeAtlasBorder(
				*FString::Printf(TEXT("NotificationSettingsRow_%d"), Index + 1),
				CommonAtlasPath,
				CommonPanelRect,
				CommonAtlasSize,
				FVector2D(1019.0f, 98.0f),
				CommonPanelMargin),
			FVector2D(0.0f, Y),
			FVector2D(1019.0f, 98.0f),
			1);
		UTextBlock* Label = MakeScreenText(
			*FString::Printf(TEXT("NotificationSettingsLabel_%d"), Index + 1),
			20,
			FLinearColor::White,
			true);
		Label->SetText(NotificationLabels[Index]);
		PlaceOnCanvas(
			Content,
			Label,
			FVector2D(34.0f, Y + 18.0f),
			FVector2D(600.0f, 34.0f),
			3);
		UTextBlock* Unknown = MakeScreenText(
			*FString::Printf(TEXT("NotificationSettingsUnknown_%d"), Index + 1),
			14,
			FLinearColor(0.70f, 0.66f, 0.54f, 1.0f));
		Unknown->SetText(NSLOCTEXT(
			"RokNotificationSettings",
			"UnknownState",
			"未同步"));
		PlaceOnCanvas(
			Content,
			Unknown,
			FVector2D(34.0f, Y + 53.0f),
			FVector2D(180.0f, 28.0f),
			3);
		URokUnityIndexedButton* Toggle = MakeActionButton(
			*FString::Printf(TEXT("NotificationSetting_%d"), Index + 1),
			1470 + Index,
			CommonAtlasPath,
			SettingSwitchOffRect,
			CommonAtlasSize,
			FVector2D(82.0f, 40.0f));
		Toggle->SetIsEnabled(false);
		PlaceOnCanvas(
			Content,
			Toggle,
			FVector2D(890.0f, Y + 29.0f),
			FVector2D(82.0f, 40.0f),
			4);
	}
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Rok notification settings: rows=10 state=unknown authority=server optimistic_update=0."));
}

void URokUnityFeatureScreenWidget::BuildLanguageSettings()
{
	if (const URokUiPageWidget* Page = MountModePage())
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(LanguageOptionButtonNames); ++Index)
		{
			BindPageAction(*Page, LanguageOptionButtonNames[Index], 1460 + Index);
		}
		BindPageAction(*Page, TEXT("LanguageConfirmButton"), 1464);
		UpdateLanguageDraftSelection();
		return;
	}
	using namespace RokLanguageSettingsLayout;
	// TypeMid is a sibling of rect, not its child: the -25.9 list offset does not move this frame.
	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("LanguageSettingsSourceFrame"), FrameAtlasPath,
		SpriteRect(1,1483,898,564), FrameAtlasSize, FrameSize()), FramePosition(), FrameSize(), 1);
	auto* Title = MakeScreenText(TEXT("FeatureFrameTitle"),23,FLinearColor::White,false,ETextJustify::Center);
	auto TitleFont=Title->GetFont(); TitleFont.Size=22.5f; Title->SetFont(TitleFont); Title->SetShadowOffset(FVector2D::ZeroVector);
	Title->SetText(NSLOCTEXT("RokLanguageSettings","SourceTitle","选择语言"));
	auto* TitleRegion=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("LanguageSettingsTitleRegion"));
	TitleRegion->SetPadding(FMargin(0)); TitleRegion->SetBrushColor(FLinearColor::Transparent); TitleRegion->SetVerticalAlignment(VAlign_Center);
	TitleRegion->SetContent(Title);TitleRegion->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(ModeCanvas,TitleRegion,TitlePosition(),FVector2D(824.5,40),7);
	PlaceOnCanvas(ModeCanvas,MakeActionButton(TEXT("FeatureFrameClose"),1,CommonAtlasPath,CloseRect,CommonAtlasSize,FVector2D(69)),
		FVector2D(1095.5,118.5),FVector2D(69),10);
	auto* Viewport = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LanguageSettingsViewport"));
	Viewport->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(ModeCanvas, Viewport, ViewportPosition(), ViewportSize(), 4);
	static const TCHAR* Labels[] = { TEXT("English"), TEXT("العربية"), TEXT("简体中文"), TEXT("Türkçe") };
	for (int32 Index=0; Index<4; ++Index)
	{
		auto* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("LanguageOptionCell_%d"),Index));
		PlaceOnCanvas(Viewport, Cell, CellPosition(Index), FVector2D(388,114), 1);
		PlaceOnCanvas(Cell, MakeAtlasBorder(*FString::Printf(TEXT("LanguageOptionBackground_%d"),Index), CommonAtlasPath,
			SpriteRect(1952,906,74,67),CommonAtlasSize,FVector2D(388,114),FMargin(23.f/74,13.f/67,21.f/74,22.f/67)), FVector2D::ZeroVector,FVector2D(388,114),0);
		PlaceOnCanvas(Cell, MakeAtlasImage(*FString::Printf(TEXT("LanguageOptionRadioBase_%d"),Index),CommonAtlasPath,
			SpriteRect(611,181,44,44),CommonAtlasSize,FVector2D(44)), FVector2D(30.3,35),FVector2D(44),1);
		PlaceOnCanvas(Cell, MakeAtlasImage(*FString::Printf(TEXT("LanguageOptionRadio_%d"),Index),CommonAtlasPath,
			SpriteRect(697,231,44,44),CommonAtlasSize,FVector2D(44)), FVector2D(30.3,35),FVector2D(44),2);
		auto* Label=MakeScreenText(*FString::Printf(TEXT("LanguageOptionLabel_%d"),Index),20,FLinearColor::White,false,ETextJustify::Left);
		auto Font=Label->GetFont();Font.Size=19.5f;Label->SetFont(Font);Label->SetShadowOffset(FVector2D::ZeroVector);Label->SetText(FText::FromString(Labels[Index]));
		Label->SetFlowDirectionPreference(EFlowDirectionPreference::LeftToRight);
		// TextLayout independently mirrors justification from its BiDi base direction; retain shaping but fix the source Chinese paragraph direction.
		FShapedTextOptions TextOptions;
		TextOptions.bOverride_TextFlowDirection=true; TextOptions.TextFlowDirection=ETextFlowDirection::LeftToRight;
		Label->SetShapedTextOptions(TextOptions);
		auto* Region=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),*FString::Printf(TEXT("LanguageOptionLabelRegion_%d"),Index));
		Region->SetPadding(FMargin(0));Region->SetBrushColor(FLinearColor::Transparent);Region->SetVerticalAlignment(VAlign_Center);
		Region->SetContent(Label);Region->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(Cell,Region,FVector2D(83.015,0),FVector2D(282.37,114),3);
		PlaceOnCanvas(Cell,MakeActionButton(LanguageOptionButtonNames[Index],1460+Index,nullptr,FIntRect(),FIntPoint::ZeroValue,FVector2D(388,114)),FVector2D::ZeroVector,FVector2D(388,114),4);
	}
	auto* Gate=MakeScreenText(TEXT("LanguageSettingsResourceGate"),15,FLinearColor(0.86f,0.72f,0.42f,1),false,ETextJustify::Center);
	Gate->SetText(NSLOCTEXT("RokLanguageSettings","DraftOnly","仅预览选择，暂不应用语言更改。"));Gate->SetShadowOffset(FVector2D::ZeroVector);
	PlaceOnCanvas(ModeCanvas,Gate,FVector2D(350,520),FVector2D(740,30),6);
	PlaceOnCanvas(ModeCanvas,MakeActionButton(TEXT("LanguageConfirmButton"),1464,CommonAtlasPath,LargeBlueButtonRect,
		CommonAtlasSize,FVector2D(274,84),NSLOCTEXT("RokLanguageSettings","SourceConfirm","确定"),24,FLinearColor::FromSRGBColor(FColor(10,27,46,255)),
		ESlateBrushDrawType::Box,FMargin(.33f,0,.33f,0)),FVector2D(583,569.9),FVector2D(274,84),7);
	if (auto* Background=GetWidgetFromName(TEXT("LanguageConfirmButtonBackground")))
	{ Background->SetRenderTransformPivot(FVector2D(.5,.5)); Background->SetRenderScale(FVector2D(-1,1)); }
	if (auto* ConfirmLabel=Cast<UTextBlock>(GetWidgetFromName(TEXT("LanguageConfirmButtonLabel"))))
	{
		ConfirmLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"),24)); ConfirmLabel->SetShadowOffset(FVector2D::ZeroVector);
		if (auto* Content=Cast<UCanvasPanel>(ConfirmLabel->GetParent()))
		{
			ConfirmLabel->RemoveFromParent();
			auto* Region=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("LanguageConfirmLabelRegion"));
			Region->SetPadding(FMargin(0));Region->SetBrushColor(FLinearColor::Transparent);Region->SetVerticalAlignment(VAlign_Center);
			Region->SetContent(ConfirmLabel);Region->SetVisibility(ESlateVisibility::HitTestInvisible);
			PlaceOnCanvas(Content,Region,FVector2D(0,10),FVector2D(274,58),5);
		}
	}
	UpdateLanguageDraftSelection();
}

void URokUnityFeatureScreenWidget::UpdateLanguageDraftSelection()
{
	for (int32 Index=0;Index<4;++Index)
		if (auto* Radio=GetWidgetFromName(*FString::Printf(TEXT("LanguageOptionRadio_%d"),Index)))
			Radio->SetVisibility(LanguageDraftId==RokLanguageSettingsLayout::SourceId(Index)?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
	if (auto* Confirm=GetWidgetFromName(TEXT("LanguageConfirmButton"))) Confirm->SetIsEnabled(RokLanguageSettingsLayout::CanCloseWithoutReload(LanguageDraftId));
	if (auto* Gate=GetWidgetFromName(TEXT("LanguageSettingsResourceGate"))) Gate->SetVisibility(RokLanguageSettingsLayout::CanCloseWithoutReload(LanguageDraftId)?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
}

void URokUnityFeatureScreenWidget::BuildAccountSettings()
{
	if (const URokUiPageWidget* Page = MountModePage())
	{
		BindPageAction(*Page, TEXT("AccountDeviceBindButton"), 1480);
		BindPageAction(*Page, TEXT("AccountIggBindButton"), 1481);
		BindPageAction(*Page, TEXT("AccountChangeButton"), 1482);
		UE_LOG(LogTemp, Log, TEXT("Rok account settings: platform_session=unknown igg_sdk=missing bind=disabled change_account=disabled."));
		return;
	}
	BuildSplitWindowFrame(
		CapturePoint(133.5f, 85.5f),
		FVector2D(1173.0f, 729.0f),
		NSLOCTEXT("RokAccountSettings", "Title", "账号管理"),
		CapturePoint(1230.0f, 89.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("AccountSettingsContentShade"),
			FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(183.0f, 136.0f),
		FVector2D(1074.0f, 664.0f),
		2);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(
			TEXT("AccountSettingsBindingsPanel"),
			CommonAtlasPath,
			CommonPanelRect,
			CommonAtlasSize,
			FVector2D(647.0f, 432.0f),
			CommonPanelMargin),
		CapturePoint(205.0f, 190.0f),
		FVector2D(647.0f, 432.0f),
		3);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(
			TEXT("AccountSettingsInfoPanel"),
			CommonAtlasPath,
			CommonPanelRect,
			CommonAtlasSize,
			FVector2D(339.0f, 432.0f),
			CommonPanelMargin),
		CapturePoint(874.0f, 190.0f),
		FVector2D(339.0f, 432.0f),
		3);

	static const FName BindingNames[] = {
		TEXT("AccountDeviceBindButton"),
		TEXT("AccountIggBindButton")};
	static const FText BindingLabels[] = {
		NSLOCTEXT("RokAccountSettings", "DeviceAccount", "设备账号"),
		FText::FromString(TEXT("IGG ID"))};
	static const FIntRect BindingIcons[] = {
		LoginDeviceIconRect,
		LoginIggIdIconRect};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(BindingNames); ++Index)
	{
		const float Y = 245.0f + Index * 150.0f;
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				*FString::Printf(TEXT("AccountBindingIcon_%d"), Index),
				BuildCityLoadingAtlasPath,
				BindingIcons[Index],
				BuildCityLoadingAtlasSize,
				FVector2D(60.0f)),
			CapturePoint(245.0f, Y),
			FVector2D(60.0f),
			5);
		UTextBlock* Label = MakeScreenText(
			*FString::Printf(TEXT("AccountBindingLabel_%d"), Index),
			21,
			FLinearColor::White,
			true);
		Label->SetText(BindingLabels[Index]);
		PlaceOnCanvas(
			ModeCanvas,
			Label,
			CapturePoint(330.0f, Y + 2.0f),
			FVector2D(260.0f, 35.0f),
			5);
		UTextBlock* State = MakeScreenText(
			*FString::Printf(TEXT("AccountBindingState_%d"), Index),
			15,
			FLinearColor(0.75f, 0.67f, 0.49f, 1.0f));
		State->SetText(NSLOCTEXT(
			"RokAccountSettings",
			"SdkUnavailable",
			"IGG SDK 未接入"));
		PlaceOnCanvas(
			ModeCanvas,
			State,
			CapturePoint(330.0f, Y + 40.0f),
			FVector2D(260.0f, 28.0f),
			5);
		URokUnityIndexedButton* Bind = MakeActionButton(
			BindingNames[Index],
			1480 + Index,
			CommonAtlasPath,
			MediumBlueButtonRect,
			CommonAtlasSize,
			FVector2D(170.0f, 64.0f),
			NSLOCTEXT("RokAccountSettings", "Bind", "绑定"),
			21,
			FLinearColor::White,
			ESlateBrushDrawType::Box,
			MiniBlueMargin);
		Bind->SetIsEnabled(false);
		PlaceOnCanvas(
			ModeCanvas,
			Bind,
			CapturePoint(635.0f, Y - 2.0f),
			FVector2D(170.0f, 64.0f),
			6);
	}

	UTextBlock* AccountStateTitle = MakeScreenText(
		TEXT("AccountSettingsCurrentTitle"),
		21,
		FLinearColor(0.96f, 0.90f, 0.65f, 1.0f),
		true,
		ETextJustify::Center);
	AccountStateTitle->SetText(NSLOCTEXT(
		"RokAccountSettings",
		"CurrentAccount",
		"当前账号"));
	PlaceOnCanvas(
		ModeCanvas,
		AccountStateTitle,
		CapturePoint(910.0f, 235.0f),
		FVector2D(270.0f, 40.0f),
		5);
	UTextBlock* AccountState = MakeScreenText(
		TEXT("AccountSettingsCurrentState"),
		18,
		FLinearColor(0.78f, 0.74f, 0.63f, 1.0f),
		false,
		ETextJustify::Center);
	AccountState->SetText(NSLOCTEXT(
		"RokAccountSettings",
		"AccountUnknown",
		"平台会话未同步\n未显示假账号或绑定状态"));
	AccountState->SetAutoWrapText(true);
	PlaceOnCanvas(
		ModeCanvas,
		AccountState,
		CapturePoint(910.0f, 315.0f),
		FVector2D(270.0f, 120.0f),
		5);

	URokUnityIndexedButton* ChangeAccount = MakeActionButton(
		TEXT("AccountChangeButton"),
		1482,
		CommonAtlasPath,
		LargeBlueButtonRect,
		CommonAtlasSize,
		FVector2D(274.0f, 84.0f),
		NSLOCTEXT("RokAccountSettings", "ChangeAccount", "切换账号"),
		25,
		FLinearColor::White,
		ESlateBrushDrawType::Box,
		FMargin(0.33f, 0.0f, 0.33f, 0.0f));
	ChangeAccount->SetIsEnabled(false);
	PlaceOnCanvas(
		ModeCanvas,
		ChangeAccount,
		CapturePoint(583.0f, 675.0f),
		FVector2D(274.0f, 84.0f),
		6);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Rok account settings: platform_session=unknown igg_sdk=missing bind=disabled change_account=disabled."));
}
void URokUnityFeatureScreenWidget::BuildArmyStatistics()
{
	const UGameInstance* ArmyInstance = GetGameInstance();
	const URokServerArmySubsystem* ServerArmy = ArmyInstance
		? ArmyInstance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const auto Preview = MakeArmyStatisticsPreviewSnapshot();
	FRokServerArmySnapshot ArmySnapshot = Preview.IsSet() ? Preview.GetValue()
		: ServerArmy ? ServerArmy->GetSnapshot() : FRokServerArmySnapshot();
	if (!Preview.IsSet() && ArmySnapshot.bAvailable && !ArmySnapshot.bLoaded)
	{
		if (URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem())
		{
			if (!Catalog->IsCatalogLoaded()) Catalog->LoadCatalog();
			if (ServerArmy) ArmySnapshot = ServerArmy->GetSnapshot();
		}
	}
	bLastServerArmyAvailable = ArmySnapshot.bAvailable;
	LastServerArmySignature = ArmySnapshot.Signature;

	UClass* RowClass = ResolveFeaturePageClass(TEXT("ArmyStatisticsRow"));
	UClass* MessageClass = ResolveFeaturePageClass(TEXT("ArmyStatisticsMessage"));
	const URokUiPageWidget* Page = RowClass && MessageClass ? MountModePage() : nullptr;
	if (!Page)
	{
		BuildSplitWindowFrame(
			CapturePoint(170.0f, 73.0f),
			FVector2D(1100.0f, 754.0f),
			NSLOCTEXT("RokArmyStatistics", "Title", "部队统计"),
			CapturePoint(1230.0f, 77.0f));
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				TEXT("ArmyStatisticsContentShade"),
				FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
			CapturePoint(184.0f, 153.0f),
			FVector2D(1072.0f, 664.0f),
			2);
	}

	static const FText PageLabels[] = {
		NSLOCTEXT("RokArmyStatistics", "AllTroops", "本地汇总"),
		NSLOCTEXT("RokArmyStatistics", "InsideCity", "城内"),
		NSLOCTEXT("RokArmyStatistics", "OutsideCity", "城外")};
	for (int32 Tab = 0; Tab < UE_ARRAY_COUNT(PageLabels); ++Tab)
	{
		const bool bSelected = Tab == SelectedArmyStatisticsPage;
		const FString Prefix = FString::Printf(TEXT("ArmyStatisticsPage_%d"), Tab);
		const FText Label = ArmySnapshot.bAvailable && Tab == 0
			? NSLOCTEXT("RokArmyStatistics", "ServerAllTroops", "全部") : PageLabels[Tab];
		if (Page)
		{
			BindPageAction(*Page, *Prefix, 1600 + Tab);
			Page->SetShown(Page->Get<UCanvasPanel>(*(Prefix + TEXT("Selected"))), bSelected);
			Page->SetShown(Page->Get<UCanvasPanel>(*(Prefix + TEXT("Normal"))), !bSelected);
			SetFeaturePageText(*Page, *(Prefix + TEXT("SelectedLabel")), Label);
			SetFeaturePageText(*Page, *(Prefix + TEXT("NormalLabel")), Label);
		}
		else
		{
			PlaceOnCanvas(ModeCanvas, MakeActionButton(*Prefix, 1600 + Tab, CommonAtlasPath,
				bSelected ? PageSelectedRect : PageNormalRect, CommonAtlasSize, FVector2D(330.0f, 48.0f),
				Label, 23, bSelected ? FLinearColor::White : FLinearColor(0.72f, 0.72f, 0.70f, 1.0f),
				ESlateBrushDrawType::Box, FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
				CapturePoint(220.0f + Tab * 330.0f, 166.0f), FVector2D(330.0f, 48.0f), 6);
		}
	}

	const bool bShowSummary = ArmySnapshot.bAvailable || SelectedArmyStatisticsPage == 0;
	const bool bLocalLocationUnavailable = !ArmySnapshot.bAvailable && SelectedArmyStatisticsPage != 0;
	const bool bInsideCity = SelectedArmyStatisticsPage == 1;
	if (Page)
	{
		Page->SetPresent(Page->Get<UBorder>(TEXT("ArmyStatisticsSummaryShade")), bShowSummary);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("ArmyStatisticsTotalCount")), bShowSummary);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("ArmyStatisticsTotalPower")), bShowSummary);
		Page->SetPresent(Page->Get<UScrollBox>(TEXT("ArmyStatisticsTroopList")), !bLocalLocationUnavailable);
		Page->SetPresent(Page->Get<UBorder>(TEXT("ArmyStatisticsInsideEmptyShade")), bLocalLocationUnavailable && bInsideCity);
		Page->SetPresent(Page->Get<UBorder>(TEXT("ArmyStatisticsOutsideEmptyShade")), bLocalLocationUnavailable && !bInsideCity);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("ArmyStatisticsInsideEmpty")), bLocalLocationUnavailable && bInsideCity);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("ArmyStatisticsOutsideEmpty")), bLocalLocationUnavailable && !bInsideCity);
	}
	else if (bShowSummary)
	{
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("ArmyStatisticsSummaryShade"),
			FLinearColor(0.18f, 0.19f, 0.18f, 0.96f)), CapturePoint(220.0f, 232.0f), FVector2D(990.0f, 112.0f), 4);
	}

	const auto DrawText = [this, Page](FName Name, const FText& Text, int32 Size, const FLinearColor& Color,
		bool bBold, const FVector2D& Position, const FVector2D& Extent)
	{
		if (Page)
		{
			SetFeaturePageText(*Page, Name, Text);
		}
		else
		{
			UTextBlock* Label = MakeScreenText(Name, Size, Color, bBold);
			Label->SetText(Text);
			PlaceOnCanvas(ModeCanvas, Label, Position, Extent, 6);
		}
	};
	const auto DrawSummary = [&DrawText](const FText& Count, const FText& Power)
	{
		const FLinearColor Color(0.96f, 0.90f, 0.65f, 1.0f);
		DrawText(TEXT("ArmyStatisticsTotalCount"), Count, 25, Color, true, CapturePoint(250.0f, 252.0f), FVector2D(330.0f, 38.0f));
		DrawText(TEXT("ArmyStatisticsTotalPower"), Power, 25, Color, true, CapturePoint(600.0f, 252.0f), FVector2D(330.0f, 38.0f));
	};
	const auto DrawDataSource = [&DrawText](const FText& Source)
	{
		DrawText(TEXT("ArmyStatisticsDataSource"), Source, 17, FLinearColor(0.76f, 0.76f, 0.73f, 1.0f), false,
			CapturePoint(250.0f, 301.0f), FVector2D(850.0f, 28.0f));
	};
	const auto MakeList = [this, Page]()
	{
		if (Page)
		{
			return Page->Get<UScrollBox>(TEXT("ArmyStatisticsTroopList"));
		}
		UScrollBox* TroopList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ArmyStatisticsTroopList"));
		TroopList->SetOrientation(Orient_Vertical);
		TroopList->SetScrollBarVisibility(ESlateVisibility::Visible);
		PlaceOnCanvas(ModeCanvas, TroopList, CapturePoint(220.0f, 364.0f), FVector2D(990.0f, 370.0f), 5);
		return TroopList;
	};
	const auto DrawStack = [this, Page, RowClass](UScrollBox& List, int32 Index, const FText& Name,
		const FText& Count, const FText& Power, const FText& Source, const FText& Tooltip)
	{
		const FLinearColor Shade = Index % 2 == 0 ? FLinearColor(0.20f, 0.21f, 0.20f, 0.98f)
			: FLinearColor(0.16f, 0.17f, 0.16f, 0.98f);
		if (URokUiPageWidget* Row = Page ? CreateWidget<URokUiPageWidget>(this, RowClass) : nullptr)
		{
			Row->LegacyPrefix = TEXT("ArmyStatistics");
			Row->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
			Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			if (UCanvasPanel* Content = Row->Get<UCanvasPanel>(TEXT("Row")))
			{
				Content->SetToolTipText(Tooltip);
			}
			if (UBorder* Border = Row->Get<UBorder>(TEXT("RowShade")))
			{
				Border->SetBrushColor(Shade);
			}
			SetFeaturePageText(*Row, TEXT("RowName"), Name);
			SetFeaturePageText(*Row, TEXT("RowCount"), Count);
			SetFeaturePageText(*Row, TEXT("RowPower"), Power);
			UTextBlock* SourceLabel = Row->Get<UTextBlock>(TEXT("RowSource"));
			Row->SetPresent(SourceLabel, !Source.IsEmpty());
			if (SourceLabel)
			{
				SourceLabel->SetText(Source);
			}
			List.AddChild(WrapFixedSize(Row,
				*FString::Printf(TEXT("ArmyStatisticsPresentationSize_%d"), Index), FVector2D(965.0f, 92.0f)));
			return;
		}
		UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("ArmyStatisticsRow_%d"), Index));
		Row->SetToolTipText(Tooltip);
		List.AddChild(WrapFixedSize(Row,
			*FString::Printf(TEXT("ArmyStatisticsRowSize_%d"), Index), FVector2D(965.0f, 92.0f)));
		PlaceOnCanvas(Row, MakeSolidBorder(*FString::Printf(TEXT("ArmyStatisticsRowShade_%d"), Index), Shade),
			FVector2D::ZeroVector, FVector2D(955.0f, 84.0f), 0);
		const auto Label = [this, Row, Index](const TCHAR* Role, const FText& Text, int32 Size,
			const FLinearColor& Color, bool bBold, const FVector2D& Position, const FVector2D& Extent)
		{
			UTextBlock* Widget = MakeScreenText(*FString::Printf(TEXT("ArmyStatisticsRow%s_%d"), Role, Index), Size, Color, bBold);
			Widget->SetText(Text);
			PlaceOnCanvas(Row, Widget, Position, Extent, 2);
		};
		Label(TEXT("Name"), Name, 23, FLinearColor(0.96f, 0.90f, 0.65f, 1.0f), true, FVector2D(24.0f, 12.0f), FVector2D(450.0f, 36.0f));
		Label(TEXT("Count"), Count, 21, FLinearColor::White, true, FVector2D(520.0f, 13.0f), FVector2D(180.0f, 34.0f));
		Label(TEXT("Power"), Power, 21, FLinearColor::White, true, FVector2D(735.0f, 13.0f), FVector2D(200.0f, 34.0f));
		if (!Source.IsEmpty())
		{
			Label(TEXT("Source"), Source, 16, FLinearColor(0.72f, 0.72f, 0.70f, 1.0f), false, FVector2D(24.0f, 50.0f), FVector2D(650.0f, 28.0f));
		}
	};
	const auto SetBreakdownPresence = [Page](bool bShow)
	{
		if (Page)
		{
			Page->SetPresent(Page->Get<UTextBlock>(TEXT("ArmyStatisticsBreakdown")), bShow);
		}
	};
	if (ArmySnapshot.bAvailable)
	{
		const bool bFieldCountKnown = ArmySnapshot.bArmiesKnown
			&& !ArmySnapshot.Marches.ContainsByPredicate([](const FRokServerMarch& March)
			{
				return !March.bSoldiersKnown;
			});
		const bool bKnown = ArmySnapshot.bLoaded && (SelectedArmyStatisticsPage == 0
			? ArmySnapshot.bTotalCountKnown
			: SelectedArmyStatisticsPage == 1 ? ArmySnapshot.bCityKnown : bFieldCountKnown);
		const TArray<FRokServerSoldierStack>& Stacks = SelectedArmyStatisticsPage == 0
			? ArmySnapshot.AllSoldiers
			: SelectedArmyStatisticsPage == 1 ? ArmySnapshot.CitySoldiers : ArmySnapshot.FieldSoldiers;
		const int64 Count = SelectedArmyStatisticsPage == 0 ? ArmySnapshot.TotalCount
			: SelectedArmyStatisticsPage == 1 ? ArmySnapshot.CityCount : ArmySnapshot.FieldCount;
		const int64 Power = SelectedArmyStatisticsPage == 0 ? ArmySnapshot.TotalPower
			: SelectedArmyStatisticsPage == 1 ? ArmySnapshot.CityPower : ArmySnapshot.FieldPower;
		const bool bPowerKnown = SelectedArmyStatisticsPage == 0 ? ArmySnapshot.bTotalPowerKnown
			: SelectedArmyStatisticsPage == 1 ? ArmySnapshot.bCityPowerKnown : ArmySnapshot.bFieldPowerKnown;
		DrawSummary(bKnown ? FText::Format(NSLOCTEXT("RokArmyStatistics", "ServerCount", "兵力：{0}"), FText::AsNumber(Count))
			: NSLOCTEXT("RokArmyStatistics", "ServerCountWaiting", "兵力：等待同步"),
			bKnown && bPowerKnown ? FText::Format(NSLOCTEXT("RokArmyStatistics", "ServerPower", "战力：{0}"), FText::AsNumber(Power))
			: NSLOCTEXT("RokArmyStatistics", "ServerPowerWaiting", "战力：待同步"));
		DrawDataSource(bKnown
			? SelectedArmyStatisticsPage == 2
				? FText::Format(NSLOCTEXT("RokArmyStatistics", "ServerFieldSource",
					"当前野外部队 · 轻伤 {0} 另计入全部"), FText::AsNumber(ArmySnapshot.FieldMinorCount))
				: NSLOCTEXT("RokArmyStatistics", "ServerSource", "当前账号部队")
			: NSLOCTEXT("RokArmyStatistics", "ServerSourceWaiting", "等待服务器部队数据"));
		const bool bShowBreakdown = bKnown && SelectedArmyStatisticsPage == 0;
		SetBreakdownPresence(bShowBreakdown);
		if (bShowBreakdown)
		{
			const FText Breakdown = FText::Format(NSLOCTEXT("RokArmyStatistics", "ServerBreakdown",
				"城内 {0} · 城外 {1} · 轻伤 {2} · 晋升中 {3} · 战力不含轻伤"),
				FText::AsNumber(ArmySnapshot.CityCount), FText::AsNumber(ArmySnapshot.FieldCount),
				FText::AsNumber(ArmySnapshot.FieldMinorCount), FText::AsNumber(ArmySnapshot.UpgradingCount));
			DrawText(TEXT("ArmyStatisticsBreakdown"), Breakdown, 15, FLinearColor(0.76f, 0.76f, 0.73f, 1.0f), false,
				CapturePoint(250.0f, 327.0f), FVector2D(940.0f, 24.0f));
		}
		UScrollBox* List = MakeList();
		if (!List) return;
		if (!bKnown || Stacks.IsEmpty())
		{
			const FText Message = bKnown ? NSLOCTEXT("RokArmyStatistics", "ServerEmpty", "暂无部队")
				: NSLOCTEXT("RokArmyStatistics", "ServerWaiting", "等待服务器同步部队位置与兵力");
			if (URokUiPageWidget* Row = Page ? CreateWidget<URokUiPageWidget>(this, MessageClass) : nullptr)
			{
				Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				SetFeaturePageText(*Row, TEXT("ArmyStatisticsServerEmpty"), Message);
				List->AddChild(WrapFixedSize(Row, TEXT("ArmyStatisticsMessageSize"), FVector2D(965.0f, 90.0f)));
			}
			else
			{
				UTextBlock* Empty = MakeScreenText(TEXT("ArmyStatisticsServerEmpty"), 23,
					FLinearColor(0.84f, 0.84f, 0.80f, 1.0f), false, ETextJustify::Center);
				Empty->SetText(Message);
				List->AddChild(WrapFixedSize(Empty, TEXT("ArmyStatisticsServerEmptySize"), FVector2D(965.0f, 90.0f)));
			}
			return;
		}
		for (int32 Index = 0; Index < Stacks.Num(); ++Index)
		{
			const FRokServerSoldierStack& Stack = Stacks[Index];
			const FText Name = Stack.bCatalogKnown && !Stack.Name.IsEmpty() ? FText::FromString(Stack.Name)
				: Stack.FamilyType >= 1 && Stack.FamilyType <= 4 && Stack.Tier > 0
					? FText::Format(NSLOCTEXT("RokArmyStatistics", "UnknownSoldierName", "{0} T{1}"),
						ResolveTroopTypeLabel(Stack.FamilyType), FText::AsNumber(Stack.Tier))
					: NSLOCTEXT("RokArmyStatistics", "UnknownSoldier", "兵种待同步");
			const FText PowerLabel = Stack.bCatalogKnown
				? FText::Format(NSLOCTEXT("RokArmyStatistics", "ServerTroopPower", "战力 {0}"), FText::AsNumber(Stack.TotalPower))
				: NSLOCTEXT("RokArmyStatistics", "ServerTroopPowerUnknown", "战力待同步");
			DrawStack(*List, Index, Name,
				FText::Format(NSLOCTEXT("RokArmyStatistics", "ServerTroopCount", "数量 {0}"), FText::AsNumber(Stack.Count)),
				PowerLabel, FText::GetEmpty(), FText::AsNumber(Stack.Id));
		}
		return;
	}

	FRokUnityRuntimeSummary Summary;
	TArray<FRokUnityRuntimeTroopStack> TroopStacks;
	bool bRuntimeLoaded = false;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const URokUnityRuntimeStateSubsystem* RuntimeState =
				GameInstance->GetSubsystem<
					URokUnityRuntimeStateSubsystem>())
		{
			bRuntimeLoaded =
				RuntimeState->IsRuntimeStateLoaded();
			Summary = RuntimeState->GetSummary();
			TroopStacks =
				RuntimeState->GetStarterTroopStacks();
		}
	}
	SetBreakdownPresence(false);
	if (!bLocalLocationUnavailable)
	{
		DrawSummary(FText::Format(NSLOCTEXT("RokArmyStatistics", "TotalCountFormat", "本地预览兵力：{0}"),
				FText::AsNumber(Summary.TotalTroopCount)),
			FText::Format(NSLOCTEXT("RokArmyStatistics", "TotalPowerFormat", "本地预览战力：{0}"),
				FText::AsNumber(Summary.TotalTroopPower)));
	}
	DrawDataSource(!bRuntimeLoaded
		? NSLOCTEXT("RokArmyStatistics", "RuntimeUnavailable", "运行态尚未加载，当前显示安全空态。")
		: !bLocalLocationUnavailable
			? NSLOCTEXT("RokArmyStatistics", "BootstrapSource", "本地预览汇总：仅包含本地引导兵力；非账号部队状态。")
			: NSLOCTEXT("RokArmyStatistics", "LocationUnavailableSource", "位置归属数据未接入；不按配置兵种推断城内或城外。"));
	if (bLocalLocationUnavailable)
	{
		if (!Page)
		{
			PlaceOnCanvas(ModeCanvas, MakeSolidBorder(
				bInsideCity ? TEXT("ArmyStatisticsInsideEmptyShade") : TEXT("ArmyStatisticsOutsideEmptyShade"),
				FLinearColor(0.13f, 0.14f, 0.13f, 0.96f)), CapturePoint(220.0f, 364.0f), FVector2D(990.0f, 370.0f), 4);
			UTextBlock* Empty = MakeScreenText(bInsideCity ? TEXT("ArmyStatisticsInsideEmpty") : TEXT("ArmyStatisticsOutsideEmpty"),
				23, FLinearColor(0.84f, 0.84f, 0.80f, 1.0f), false, ETextJustify::Center);
			Empty->SetText(bInsideCity ? NSLOCTEXT("RokArmyStatistics", "InsideLocationUnavailable",
					"城内兵力位置数据尚未接入。\n当前无法按城内归属显示部队。")
				: NSLOCTEXT("RokArmyStatistics", "OutsideLocationUnavailable",
					"城外兵力位置数据尚未接入。\n当前无法按城外归属显示部队。"));
			Empty->SetAutoWrapText(true);
			PlaceOnCanvas(ModeCanvas, Empty, CapturePoint(345.0f, 490.0f), FVector2D(740.0f, 120.0f), 6);
		}
		return;
	}
	UScrollBox* List = MakeList();
	if (!List) return;
	for (int32 Index = 0; Index < TroopStacks.Num(); ++Index)
	{
		const FRokUnityRuntimeTroopStack& Stack = TroopStacks[Index];
		DrawStack(*List, Index,
			FText::Format(NSLOCTEXT("RokArmyStatistics", "TroopNameFormat", "{0} T{1}  ·  配置 {2}"),
				ResolveTroopTypeLabel(Stack.ArmsType), FText::AsNumber(Stack.Level), FText::AsNumber(Stack.TroopId)),
			FText::Format(NSLOCTEXT("RokArmyStatistics", "TroopCountFormat", "数量 {0}"), FText::AsNumber(Stack.Count)),
			FText::Format(NSLOCTEXT("RokArmyStatistics", "TroopPowerFormat", "战力 {0}"), FText::AsNumber(Stack.TotalPower)),
			FText::Format(NSLOCTEXT("RokArmyStatistics", "TroopSourceFormat", "单兵战力 {0}  ·  训练耗时 {1} 秒"),
				FText::AsNumber(Stack.PowerPerUnit), FText::AsNumber(Stack.TrainSeconds)), FText::GetEmpty());
	}
}

#include "RokFeatureStore.inl"

#if !UE_BUILD_SHIPPING
AActor* URokUnityFeatureScreenWidget::GetFeatureContextActorForAudit() const
{
	return FeatureContextActor.Get();
}
#endif

void URokUnityFeatureScreenWidget::BuildTrainArmy()
{
	const UGameInstance* ArmyInstance = GetGameInstance();
	const URokServerArmySubsystem* ServerArmy = ArmyInstance
		? ArmyInstance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const FRokServerArmySnapshot ArmySnapshot = ServerArmy
		? ServerArmy->GetSnapshot() : FRokServerArmySnapshot();
	bLastServerArmyAvailable = ArmySnapshot.bAvailable;
	LastServerArmySignature = ArmySnapshot.Signature;
	const FRokServerTrainingBuildingView* ServerBuilding = ArmySnapshot.bAvailable
		&& ArmySnapshot.bBuildingsKnown ? FindServerTrainingBuilding(
			ArmySnapshot, SelectedServerTrainBuildingIndex, SelectedTrainArmyType) : nullptr;
	const FRokServerTrainingQueue* ServerQueue = ArmySnapshot.bAvailable && ArmySnapshot.bLoaded
		&& ArmySnapshot.bTrainingKnown && ServerBuilding
		? FindServerTrainingQueue(ArmySnapshot, SelectedServerTrainBuildingIndex) : nullptr;
	bTrainArmyQuantityEditable = false;
	bTrainCanStart = false;
	TrainArmyInstantConfirmation = nullptr;
	TrainArmyInstantPrice = -1;
	bTrainActionReceives = ServerQueue && ServerQueue->State == ERokServerTrainingState::AwaitingReceive;
	TrainArmyAnimatedChildren.Reset();
	TrainArmyTipLayer = nullptr;
	const FText ArmyLabel = ResolveTroopTypeLabel(SelectedTrainArmyType);
	const FText TitleText = FText::Format(NSLOCTEXT("RokTrainArmy", "TitleFormat", "{0}单位训练"), ArmyLabel);
	auto* SharedFrame = MountModePage(TEXT("TrainArmyFrame"));
	auto* FrameTitle = SharedFrame ? SharedFrame->Get<UTextBlock>(TEXT("FeatureFrameTitle")) : nullptr;
	if (FrameTitle)
	{
		FrameTitle->SetText(TitleText);
	}
	else
	{
		if (SharedFrame)
		{
			SharedFrame->RemoveFromParent();
			ActiveModePage = nullptr;
		}
		BuildSplitWindowFrame(FVector2D(133.5, 40.5), FVector2D(1173, 729), TitleText,
			CapturePoint(1230.0f, 77.0f), nullptr, 1, 32.0f);
		if (auto* Title = Cast<UTextBlock>(GetWidgetFromName(TEXT("FeatureFrameTitle"))))
		{
			Title->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 24));
		}
	}

	URokUnityGameplayCatalogSubsystem* GameplayCatalog =
		GetGameplayCatalogSubsystem();
	if (GameplayCatalog && !GameplayCatalog->IsCatalogLoaded())
	{
		GameplayCatalog->LoadCatalog();
	}
	URokUnityRuntimeStateSubsystem* RuntimeState =
		GetRuntimeStateSubsystem();
	if (RuntimeState && !RuntimeState->IsRuntimeStateLoaded())
	{
		RuntimeState->LoadRuntimeState();
	}

	int32 BuildingLevel = 1;
	FText BuildingName = FText::Format(
		NSLOCTEXT(
			"RokTrainArmy",
			"PreviewBuildingFormat",
			"{0}预览"),
		ArmyLabel);
	if (FeatureContextActor.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (URokBuildingUpgradeSubsystem* UpgradeSubsystem =
					World->GetSubsystem<URokBuildingUpgradeSubsystem>())
			{
				const FRokBuildingState State = UpgradeSubsystem
					->GetBuildingStateForActor(FeatureContextActor.Get());
				BuildingLevel = FMath::Max(1, State.Level);
				if (!State.DisplayName.IsEmpty())
				{
					BuildingName = State.DisplayName;
				}
			}
		}
	}
	if (ArmySnapshot.bAvailable)
	{
		BuildingLevel = ServerBuilding ? ServerBuilding->Level : 0;
		BuildingName = ServerBuilding
			? FText::FromString(ServerBuilding->Name.IsEmpty() ? TEXT("训练建筑") : ServerBuilding->Name)
			: NSLOCTEXT("RokTrainArmy", "ServerBuildingWaiting", "训练建筑待同步");
	}

	const int32 BuildingTypeId = SelectedTrainArmyType + 8;
	FRokUnityCatalogBuildingType BuildingType;
	const bool bHasBuildingType = (!ArmySnapshot.bAvailable || BuildingLevel > 0) && GameplayCatalog
		&& GameplayCatalog->GetBuildingType(
			BuildingTypeId,
			BuildingType);
	const int32 BaseCapacity = bHasBuildingType ? BuildingType.GetTrainingCapacity(BuildingLevel) : 0;
	const int32 TrainingCapacity = BaseCapacity > 0 && ArmySnapshot.bAvailable && ServerArmy
		? static_cast<int32>(FMath::Min<int64>(int64(BaseCapacity) + ServerArmy->GetTrainingCapacityBonus(), MAX_int32))
		: BaseCapacity;
	TrainArmyBaseCapacity = TrainingCapacity;
	const int32 SafeTrainingCapacity = FMath::Max(1, TrainingCapacity);
	SelectedTrainCount = FMath::Clamp(
		SelectedTrainCount,
		1,
		SafeTrainingCapacity);

	TArray<FRokUnityCatalogTroop> Troops = GameplayCatalog
		? GameplayCatalog->GetTroopsForArmyType(
			SelectedTrainArmyType)
		: TArray<FRokUnityCatalogTroop>();
	const FRokUnityCatalogTroop* SelectedTroop = Troops.FindByPredicate(
		[this](const FRokUnityCatalogTroop& Troop)
		{
			return Troop.Level == SelectedTrainTier;
		});
	if (!SelectedTroop && !Troops.IsEmpty())
	{
		SelectedTrainTier = Troops[0].Level;
		SelectedTroop = &Troops[0];
	}

	FRokUnityCatalogTroop ServerTroop;
	if (ServerArmy && ArmySnapshot.bAvailable && ServerArmy->GetTrainingTroop(SelectedTrainArmyType, SelectedTrainTier, ServerTroop))
		SelectedTroop = &ServerTroop;
	const bool bTierUnlocked = SelectedTroop && (SelectedTroop->StudyId == 0
		|| (ServerArmy && ServerArmy->IsTrainingTierUnlocked(SelectedTroop->StudyId)));
	if (SelectedTroop) BuildTrainArmySharedDisplay(SelectedTroop->Id);
	if (bTrainArmyDetailVisible && SelectedTroop)
	{
		BuildTrainArmyDetail(SelectedTroop->Id);
		return;
	}
	const bool bIdle = !ArmySnapshot.bAvailable || (ServerBuilding && ArmySnapshot.bLoaded
		&& ArmySnapshot.bTrainingKnown && (!ServerQueue || ServerQueue->State == ERokServerTrainingState::Idle));
	bTrainArmyQuantityEditable = bTierUnlocked && TrainArmyBaseCapacity > 0 && bIdle;
	bTrainCanStart = bTrainArmyQuantityEditable;
	const FText BuildingContextFormat = ArmySnapshot.bAvailable
		? NSLOCTEXT("RokTrainArmy", "ServerBuildingContextFormat", "{0}  ·  等级 {1}  ·  可选数量 {2}")
		: NSLOCTEXT("RokTrainArmy", "BuildingContextFormat", "{0}  ·  等级 {1}  ·  单队容量 {2}");
	const FText ContextText = ArmySnapshot.bAvailable && BuildingLevel <= 0
		? NSLOCTEXT("RokTrainArmy", "ServerBuildingContextWaiting", "训练建筑状态待同步")
		: FText::Format(BuildingContextFormat,
		BuildingName,
		FText::AsNumber(BuildingLevel),
		TrainingCapacity > 0
			? FText::AsNumber(TrainingCapacity)
			: NSLOCTEXT(
				"RokTrainArmy",
				"CapacityUnavailable",
				"未载入"));
	if (SelectedTroop && BuildTrainArmyParameterPage(*SelectedTroop, Troops, ArmySnapshot, ServerArmy,
		ServerQueue, ContextText, bTierUnlocked, bIdle))
	{
		RefreshTrainArmyAction();
		AdvanceTrainArmyDetailAnimation(0.0f);
		return;
	}
	const int32 TrainFirstChild = ModeCanvas->GetChildrenCount();
	UTextBlock* BuildingContext = MakeScreenText(
		TEXT("TrainArmyBuildingContext"),
		19,
		FLinearColor(0.82f, 0.81f, 0.75f, 1.0f),
		false,
		ETextJustify::Center);
	BuildingContext->SetText(ContextText);
	PlaceOnCanvas(
		ModeCanvas,
		BuildingContext,
		CapturePoint(190.0f, 151.0f),
		FVector2D(1060.0f, 34.0f),
		5);

	// Reuse the source grayscale UI material with the troop atlas UV region.
	UMaterialInstanceDynamic* GrayPortrait = nullptr;
	if (auto* Gray = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/RokPrototype/UI/Research/M_RokResearchGrayUI.M_RokResearchGrayUI")))
	{
		GrayPortrait = UMaterialInstanceDynamic::Create(Gray, this);
		GrayPortrait->SetTextureParameterValue(TEXT("SourceAtlas"), LoadUiTexture(TroopHeadAtlasPath));
	}
	for (int32 TierIndex = 0; TierIndex < 5; ++TierIndex)
	{
		const int32 Tier = TierIndex + 1;
		const FRokUnityCatalogTroop* TierTroop = Troops.FindByPredicate(
			[Tier](const FRokUnityCatalogTroop& Troop)
			{
				return Troop.Level == Tier;
			});
		const bool bSelected = SelectedTrainTier == Tier;
		const bool bNoStudyRequired = TierTroop && (TierTroop->StudyId == 0
			|| (ServerArmy && ServerArmy->IsTrainingTierUnlocked(TierTroop->StudyId)));
		const FVector2D Position = CapturePoint(
			220.0f + TierIndex * 202.0f,
			199.0f);
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				*FString::Printf(TEXT("TrainArmyTierQuality_%d"), TierIndex),
				Item0AtlasPath,
				bSelected ? ItemSelectedRect : ItemQualityCommonRect,
				Item0AtlasSize,
				FVector2D(132.0f)),
			Position,
			FVector2D(132.0f),
			5);
		FIntRect TroopRect;
		if (ResolveTroopHeadRect(
			SelectedTrainArmyType,
			Tier,
			TroopRect))
		{
			auto* Portrait = MakeAtlasImage(*FString::Printf(TEXT("TrainArmyTierIcon_%d"), TierIndex),
				TroopHeadAtlasPath, TroopRect, TroopHeadAtlasSize, FVector2D(116));
			if (!bNoStudyRequired && GrayPortrait)
			{
				FSlateBrush Brush = Portrait->GetBrush();
				Brush.SetResourceObject(GrayPortrait);
				Portrait->SetBrush(Brush);
			}
			PlaceOnCanvas(ModeCanvas, Portrait, Position + FVector2D(8), FVector2D(116), 6);
		}
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(TEXT("TrainArmyTier_%d"), TierIndex),
				1900 + TierIndex,
				nullptr,
				FIntRect(),
				FIntPoint(1, 1),
				FVector2D(132.0f)),
			Position,
			FVector2D(132.0f),
			9);

		UTextBlock* TierLabel = MakeScreenText(
			*FString::Printf(TEXT("TrainArmyTierLabel_%d"), TierIndex),
			20,
			bNoStudyRequired
				? FLinearColor(0.98f, 0.91f, 0.56f, 1.0f)
				: FLinearColor(0.72f, 0.72f, 0.72f, 1.0f),
			true,
			ETextJustify::Center);
		int64 TierOwned = 0;
		for (const FRokServerSoldierStack& Stack : ArmySnapshot.CitySoldiers)
			if (Stack.FamilyType == SelectedTrainArmyType && Stack.Tier == Tier) TierOwned += Stack.Count;
		TierLabel->SetText(ArmySnapshot.bCityKnown ? FText::AsNumber(TierOwned) : FText::FromString(TEXT("—")));
		TierLabel->SetColorAndOpacity(bNoStudyRequired ? FLinearColor::White : FLinearColor(.35f, .35f, .34f));
		PlaceOnCanvas(
			ModeCanvas,
			TierLabel,
			Position + FVector2D(-14.0f, 135.0f),
			FVector2D(160.0f, 34.0f),
			10);
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("TrainArmySelectedShade"),
			FLinearColor(0.08f, 0.09f, 0.075f, 0.98f)),
		CapturePoint(198.0f, 385.0f),
		FVector2D(1020.0f, 214.0f),
		4);
	if (SelectedTroop)
	{

		int64 BootstrapOwned = 0;
		if (ArmySnapshot.bAvailable)
		{
			if (ArmySnapshot.bCityKnown)
			{
				for (const FRokServerSoldierStack& Stack : ArmySnapshot.CitySoldiers)
				{
					if (Stack.Id == SelectedTroop->Id) BootstrapOwned += Stack.Count;
				}
			}
		}
		else if (RuntimeState)
		{
			for (const FRokUnityRuntimeTroopStack& Stack :
				RuntimeState->GetStarterTroopStacks())
			{
				if (Stack.TroopId == SelectedTroop->Id)
				{
					BootstrapOwned += Stack.Count;
				}
			}
		}
		UTextBlock* TroopName = MakeScreenText(
			TEXT("TrainArmySelectedName"),
			25,
			FLinearColor(0.98f, 0.91f, 0.56f, 1.0f),
			true);
		FRokTrainArmyDetail SelectedDetail;
		FString DetailError;
		const bool bHasSourceName = RokTrainArmyDetail::Load(SelectedTroop->Id, SelectedDetail, DetailError)
			&& !SelectedDetail.Name.IsEmpty();
		TroopName->SetText(FText::Format(
			NSLOCTEXT(
				"RokTrainArmy",
				"SelectedTroopSourceNameFormat",
				"{0} T{1}"),
			bHasSourceName ? FText::FromString(SelectedDetail.Name) : ArmyLabel,
			FText::AsNumber(SelectedTroop->Level)));
		PlaceOnCanvas(
			ModeCanvas,
			TroopName,
			CapturePoint(425.0f, 403.0f),
			FVector2D(750.0f, 42.0f),
			7);

		UTextBlock* Stats = MakeScreenText(
			TEXT("TrainArmySelectedStats"),
			19,
			FLinearColor(0.88f, 0.88f, 0.84f, 1.0f),
			false);
		Stats->SetText(FText::Format(
			NSLOCTEXT(
				"RokTrainArmy",
				"StatsFormat",
				"攻击 {0}    防御 {1}    生命 {2}    行军速度 {3}    战力 {4}"),
			FText::AsNumber(SelectedTroop->Attack),
			FText::AsNumber(SelectedTroop->Defense),
			FText::AsNumber(SelectedTroop->Hp),
			FText::AsNumber(SelectedTroop->Speed),
			FText::AsNumber(SelectedTroop->Power)));
		PlaceOnCanvas(
			ModeCanvas,
			Stats,
			CapturePoint(425.0f, 455.0f),
			FVector2D(750.0f, 36.0f),
			7);
		const FRokResourceCost TotalCost = ScaleResourceCost(
			SelectedTroop->TrainCost,
			SelectedTrainCount);
		const int32 TotalSeconds = static_cast<int32>(FMath::Clamp<int64>(
			static_cast<int64>(SelectedTroop->TrainSeconds)
				* SelectedTrainCount,
			0,
			MAX_int32));
		UTextBlock* CostAndTime = MakeScreenText(
			TEXT("TrainArmyCostAndTime"),
			18,
			FLinearColor(0.82f, 0.82f, 0.78f, 1.0f),
			false);
		CostAndTime->SetText(ArmySnapshot.bAvailable
			? FText::Format(NSLOCTEXT("RokTrainArmy", "ServerCostTimeFormat",
				"参考消耗：{0}    基础训练时间：{1}\n城内拥有：{2}"),
				FormatBuildCost(TotalCost), FormatBuildDuration(TotalSeconds),
				ArmySnapshot.bCityKnown ? FText::AsNumber(BootstrapOwned)
					: NSLOCTEXT("RokTrainArmy", "ServerOwnedWaiting", "待同步"))
			: FText::Format(NSLOCTEXT("RokTrainArmy", "CostTimeFormat",
				"总消耗：{0}    训练时间：{1}\n本地启动快照拥有：{2}（仅迁移验收，不是服务器库存）"),
				FormatBuildCost(TotalCost), FormatBuildDuration(TotalSeconds), FText::AsNumber(BootstrapOwned)));
		CostAndTime->SetAutoWrapText(true);
		PlaceOnCanvas(
			ModeCanvas,
			CostAndTime,
			CapturePoint(425.0f, 505.0f),
			FVector2D(750.0f, 80.0f),
			7);
	}
	else if (!TryBuildTrainArmyUnavailableState(TEXT("CatalogMissing"), ModeCanvas))
	{
		UTextBlock* MissingCatalog = MakeScreenText(
			TEXT("TrainArmyMissingCatalog"),
			22,
			FLinearColor(0.95f, 0.65f, 0.45f, 1.0f),
			true,
			ETextJustify::Center);
		MissingCatalog->SetText(NSLOCTEXT(
			"RokTrainArmy",
			"MissingCatalog",
			"Unity 兵种目录未载入，不能生成训练预览。"));
		PlaceOnCanvas(
			ModeCanvas,
			MissingCatalog,
			CapturePoint(300.0f, 470.0f),
			FVector2D(820.0f, 50.0f),
			7);
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("TrainArmyMinus"),
			1920,
			CommonAtlasPath,
			ItemMinusRect,
			CommonAtlasSize,
			FVector2D(50.0f),
			FText::FromString(TEXT("−")),
			22),
		CapturePoint(320.0f, 637.0f),
		FVector2D(50.0f),
		8);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("TrainArmyQuantityShade"),
			FLinearColor(0.10f, 0.11f, 0.095f, 0.98f)),
		CapturePoint(379.0f, 639.0f),
		FVector2D(190.0f, 47.0f),
		6);
	UTextBlock* Quantity = MakeScreenText(
		TEXT("TrainArmyQuantity"),
		23,
		FLinearColor::White,
		true,
		ETextJustify::Center);
	Quantity->SetText(FText::Format(
		NSLOCTEXT("RokTrainArmy", "QuantityFormat", "数量 {0}"),
		FText::AsNumber(SelectedTrainCount)));
	PlaceOnCanvas(
		ModeCanvas,
		Quantity,
		CapturePoint(379.0f, 645.0f),
		FVector2D(190.0f, 34.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("TrainArmyPlus"),
			1921,
			CommonAtlasPath,
			ItemPlusRect,
			CommonAtlasSize,
			FVector2D(50.0f),
			FText::FromString(TEXT("+")),
			22),
		CapturePoint(578.0f, 637.0f),
		FVector2D(50.0f),
		8);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("TrainArmyMax"),
			1922,
			CommonAtlasPath,
			MiniBlueButtonRect,
			CommonAtlasSize,
			FVector2D(105.0f, 50.0f),
			NSLOCTEXT("RokTrainArmy", "Max", "最大"),
			19),
		CapturePoint(645.0f, 637.0f),
		FVector2D(105.0f, 50.0f),
		8);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("TrainArmyStart"),
			1940,
			CommonAtlasPath,
			MediumGoldButtonRect,
			CommonAtlasSize,
			FVector2D(250.0f, 76.0f),
			ArmySnapshot.bAvailable
				? NSLOCTEXT("RokTrainArmy", "ServerTrainingUnavailable", "训练待确认")
				: NSLOCTEXT("RokTrainArmy", "StartTraining", "开始训练"),
			27,
			FLinearColor(0.25f, 0.13f, 0.02f, 1.0f),
			ESlateBrushDrawType::Box,
			FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		CapturePoint(905.0f, 622.0f),
		FVector2D(250.0f, 76.0f),
		9);
	BuildTrainArmyQuantityControls();
	BuildTrainResourceCost();
	if (ArmySnapshot.bAvailable)
	{
		UTextBlock* QueueStatus = MakeScreenText(TEXT("TrainArmyServerQueueStatus"), 18,
			FLinearColor(0.95f, 0.88f, 0.62f, 1.0f));
		QueueStatus->SetText(DescribeServerTrainingQueue(ArmySnapshot,
			SelectedServerTrainBuildingIndex, SelectedTrainArmyType));
		QueueStatus->SetVisibility(bIdle ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(ModeCanvas, QueueStatus, CapturePoint(240.0f, 575.0f), FVector2D(960.0f, 39.0f), 9);
		UProgressBar* QueueProgress = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), TEXT("TrainArmyServerQueueProgress"));
		QueueProgress->SetPercent(ServerQueue && ServerQueue->bTimeKnown
			&& (ServerQueue->State == ERokServerTrainingState::Training
				|| ServerQueue->State == ERokServerTrainingState::Upgrading)
			? FMath::Clamp(static_cast<float>(ServerQueue->Progress01), 0.0f, 1.0f) : 0.0f);
		QueueProgress->SetVisibility(ServerQueue && ServerQueue->bTimeKnown
			&& (ServerQueue->State == ERokServerTrainingState::Training || ServerQueue->State == ERokServerTrainingState::Upgrading)
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		PlaceOnCanvas(ModeCanvas, QueueProgress, CapturePoint(240.0f, 635.0f), FVector2D(960.0f, 8.0f), 9);
	}
	// Locked tiers remain browseable. A completed building queue can be
	// collected even while the player is previewing another tier.
	if (SelectedTroop && !bTierUnlocked && !bTrainActionReceives)
	{
		const FName HiddenControls[] = { TEXT("TrainArmyCostAndTime"), TEXT("TrainArmyMinus"),
			TEXT("TrainArmyQuantityShade"), TEXT("TrainArmyQuantity"), TEXT("TrainArmyPlus"),
			TEXT("TrainArmyMax"), TEXT("TrainArmyStart"), TEXT("TrainArmyQuantityInput"),
			TEXT("TrainArmyQuantitySlider"), TEXT("TrainArmyQuantitySliderBackground"), TEXT("TrainArmyQuantitySliderFill"), TEXT("TrainResourceCostRoot") };
		for (const FName& Name : HiddenControls)
			if (UWidget* Child = GetWidgetFromName(Name)) Child->SetVisibility(ESlateVisibility::Collapsed);
		auto* Status = MakeScreenText(TEXT("TrainArmyStudyStatus"), 20, FLinearColor(0.92f, 0.85f, 0.65f, 1.0f));
		Status->SetAutoWrapText(true);
		Status->SetText(ArmySnapshot.bAvailable
			? NSLOCTEXT("RokTrainArmy", "StudyStatusRequired", "当前兵阶需要完成对应科技。可查看兵种详情。")
			: NSLOCTEXT("RokTrainArmy", "StudyStatusUnknown", "当前兵阶需要科技解锁，解锁状态待同步。可查看兵种详情。"));
		PlaceOnCanvas(ModeCanvas, Status, CapturePoint(425, 515), FVector2D(750, 80), 8);
	}
	for (int32 Index = TrainFirstChild; Index < ModeCanvas->GetChildrenCount(); ++Index)
	{
		auto* Child = ModeCanvas->GetChildAt(Index);
		if (Child->GetFName() != TEXT("TrainArmyBuildingContext")) TrainArmyAnimatedChildren.Add(Child);
	}
	if (SelectedTroop)
		PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("TrainArmyDetailButton"), 1950, CommonAtlasPath,
			MediumGoldButtonRect, CommonAtlasSize, FVector2D(130, 48), FText::FromString(TEXT("详情")), 23),
			CapturePoint(1065, 730), FVector2D(130, 48), 10);
	if (SelectedTroop) ApplyTrainParameterLayout(SelectedTroop->Id);
	RefreshTrainArmyAction();
	AdvanceTrainArmyDetailAnimation(0.0f);
}

void URokUnityFeatureScreenWidget::RefreshTrainArmyAction()
{
	auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("TrainArmyStart")));
	auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainArmyStartLabel")));
	if (!Button || !Label) return;
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	const bool bPending = Net && Net->IsTrainingWritePending();
	Button->SetIsEnabled(!bPending && (bTrainCanStart || bTrainActionReceives));
	Label->SetText(bPending ? NSLOCTEXT("RokTrainArmy", "Sending", "提交中…")
		: bTrainActionReceives ? NSLOCTEXT("RokTrainArmy", "ReceiveTrainedArmy", "领取")
		: NSLOCTEXT("RokTrainArmy", "TrainAction", "训练"));
	TrainArmyInstantPrice = -1;
	if (auto* Instant = Cast<UButton>(GetWidgetFromName(TEXT("TrainArmyInstant"))))
	{
		const auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
		FRokUnityCatalogTroop Troop;
		const bool bHasPrice = Army && Army->GetTrainingTroop(SelectedTrainArmyType, SelectedTrainTier, Troop)
			&& Army->GetInstantTrainingPrice(Troop, SelectedTrainCount, TrainArmyInstantPrice);
		Instant->SetIsEnabled(!bPending && bHasPrice);
		if (auto* Price = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainArmyInstantPrice"))))
			Price->SetText(bHasPrice ? FText::AsNumber(TrainArmyInstantPrice) : FText::FromString(TEXT("—")));
	}
}

void URokUnityFeatureScreenWidget::OpenTrainArmyInstantConfirmation()
{
	if (!bTrainCanStart || bTrainArmyDetailVisible || TrainArmyInstantConfirmation) return;
	RefreshTrainArmyAction();
	if (TrainArmyInstantPrice < 0) return;
	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	int64 Gems = 0;
	if (!Net || !RokSprotoJson::TryGetInt64(Net->GetRoleInfo(), TEXT("denar"), Gems) || Gems < TrainArmyInstantPrice)
	{
		ShowBoundaryFeedback(NSLOCTEXT("RokTrainArmy", "InstantBalance", "宝石不足或余额待同步。"));
		return;
	}
	TrainArmyConfirmationPrice = TrainArmyInstantPrice;
	const FText Message = FText::Format(NSLOCTEXT("RokTrainArmy", "InstantConfirmText",
		"预计消耗 {0} 宝石，立即训练 {1} 名士兵。\n实际费用由服务器结算。"),
		FText::AsNumber(TrainArmyConfirmationPrice), FText::AsNumber(SelectedTrainCount));
	UClass* ConfirmClass = ActiveModePage ? ResolveFeaturePageClass(TEXT("TrainArmyInstantConfirm")) : nullptr;
	if (auto* Dialog = ConfirmClass ? CreateWidget<URokUiPageWidget>(this, ConfirmClass) : nullptr)
	{
		if (auto* Text = Dialog->Get<UTextBlock>(TEXT("TrainArmyConfirmText")); Text
			&& Dialog->Get<URokUnityIndexedButton>(TEXT("TrainArmyInstantCancel"))
			&& Dialog->Get<URokUnityIndexedButton>(TEXT("TrainArmyInstantConfirm")))
		{
			Text->SetText(Message);
			BindPageAction(*Dialog, TEXT("TrainArmyInstantCancel"), 1942);
			BindPageAction(*Dialog, TEXT("TrainArmyInstantConfirm"), 1943);
			Dialog->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			PlaceOnCanvas(ModeCanvas, Dialog, FVector2D::ZeroVector, CurrentExpandedDesignSize, 90);
			TrainArmyInstantConfirmation = Dialog;
			return;
		}
	}

	auto* ConfirmationCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TrainArmyInstantConfirmation"));
	TrainArmyInstantConfirmation = ConfirmationCanvas;
	PlaceOnCanvas(ModeCanvas, ConfirmationCanvas, FVector2D::ZeroVector, CurrentExpandedDesignSize, 90);
	auto* Shade = MakeSolidBorder(TEXT("TrainArmyConfirmMask"), FLinearColor(0, 0, 0, .68f));
	Shade->SetVisibility(ESlateVisibility::Visible);
	PlaceOnCanvas(ConfirmationCanvas, Shade, FVector2D::ZeroVector, CurrentExpandedDesignSize);
	const FVector2D Size(538, 200);
	const FVector2D Origin = (CurrentExpandedDesignSize - Size) * .5f;
	PlaceOnCanvas(ConfirmationCanvas, MakeAtlasBorder(TEXT("TrainArmyConfirmFrame"), FrameAtlasPath,
		WindowBodyRect, FrameAtlasSize, Size, WindowBodyMargin), Origin, Size, 1);
	auto* Text = MakeScreenText(TEXT("TrainArmyConfirmText"), 22, FLinearColor::White, false, ETextJustify::Center);
	Text->SetText(Message);
	PlaceOnCanvas(ConfirmationCanvas, Text, Origin + FVector2D(22, 29), FVector2D(494, 85), 2);
	PlaceOnCanvas(ConfirmationCanvas, MakeActionButton(TEXT("TrainArmyInstantCancel"), 1942,
		CommonAtlasPath, MediumBlueButtonRect, CommonAtlasSize, FVector2D(175, 62),
		NSLOCTEXT("RokTrainArmy", "InstantCancel", "取消"), 22), Origin + FVector2D(74, 120), FVector2D(175, 62), 3);
	PlaceOnCanvas(ConfirmationCanvas, MakeActionButton(TEXT("TrainArmyInstantConfirm"), 1943,
		CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize, FVector2D(175, 62),
		NSLOCTEXT("RokTrainArmy", "InstantConfirm", "确认"), 22), Origin + FVector2D(289, 120), FVector2D(175, 62), 3);
}

void URokUnityFeatureScreenWidget::CloseTrainArmyInstantConfirmation(bool bConfirm)
{
	if (!TrainArmyInstantConfirmation) return;
	TrainArmyInstantConfirmation->RemoveFromParent();
	TrainArmyInstantConfirmation = nullptr;
	if (bConfirm) SubmitTrainArmy(TrainArmyConfirmationPrice);
}

void URokUnityFeatureScreenWidget::SubmitTrainArmy(TOptional<int64> ConfirmedGems)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::TrainArmy || bTrainArmyDetailVisible
		|| TrainArmyInstantConfirmation || (!bTrainCanStart && !bTrainActionReceives)) return;
	auto* Instance = GetGameInstance();
	auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const bool bReceive = bTrainActionReceives;
	const bool bInstant = ConfirmedGems.IsSet();
	if (bInstant && !bTrainCanStart) return;
	const int32 BuildingIndex = SelectedServerTrainBuildingIndex;
	const int32 ArmyType = SelectedTrainArmyType;
	auto OnResult = [WeakThis = TWeakObjectPtr<URokUnityFeatureScreenWidget>(this), BuildingIndex, ArmyType, bReceive, bInstant]
		(const FString& Error)
	{
		auto* Screen = WeakThis.Get();
		if (!Screen || Screen->ActiveMode != ERokUnityFeatureScreenMode::TrainArmy
			|| Screen->SelectedServerTrainBuildingIndex != BuildingIndex || Screen->SelectedTrainArmyType != ArmyType) return;
		Screen->RefreshServerArmyPresentation();
		Screen->RefreshTrainArmyAction();
		Screen->ShowBoundaryFeedback(Error.IsEmpty()
			? (bReceive || bInstant ? NSLOCTEXT("RokTrainArmy", "ReceiveConfirmed", "训练部队已入城。")
				: NSLOCTEXT("RokTrainArmy", "TrainConfirmed", "已开始训练。"))
			: FText::FromString(Error));
	};
	FString Error;
	const bool bSent = Army && (bReceive
		? Army->RequestTrainingAward(BuildingIndex, ArmyType, MoveTemp(OnResult), Error)
		: Army->RequestTraining(BuildingIndex, ArmyType, SelectedTrainTier, SelectedTrainCount, MoveTemp(OnResult), Error, ConfirmedGems));
	RefreshTrainArmyAction();
	ShowBoundaryFeedback(bSent ? NSLOCTEXT("RokTrainArmy", "WaitingForServer", "等待服务器确认…")
		: FText::FromString(Error.IsEmpty() ? TEXT("连接服务器后可训练或领取部队。") : Error));
}

void URokUnityFeatureScreenWidget::BuildTrainArmyQuantityControls()
{
	auto* Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), TEXT("TrainArmyQuantityInput"));
	Input->SetText(FText::FromString(FString::FromInt(SelectedTrainCount)));
	Input->SetJustification(ETextJustify::Center);
	// Unity hides Input.textComponent and displays lbl_count_format above it.
	Input->SetForegroundColor(FLinearColor(1, 1, 1, 0));
	FEditableTextBoxStyle InputStyle = Input->GetWidgetStyle();
	InputStyle.SetFocusedForegroundColor(FLinearColor(1, 1, 1, 0));
	InputStyle.SetBackgroundColor(FLinearColor::Transparent);
	Input->SetWidgetStyle(InputStyle);
	Input->SetIsEnabled(bTrainArmyQuantityEditable);
	Input->OnTextChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleTrainArmyQuantityTextChanged);
	PlaceOnCanvas(ModeCanvas, Input, CapturePoint(379, 639), FVector2D(190, 47), 8);
	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("TrainArmyQuantitySliderBackground"), CommonAtlasPath,
		SpriteRect(401, 112, 20, 23), CommonAtlasSize, FVector2D(460, 23), FMargin(.4f)),
		CapturePoint(320, 710), FVector2D(460, 23), 6);
	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("TrainArmyQuantitySliderFill"), CommonAtlasPath,
		SpriteRect(384, 59, 12, 19), CommonAtlasSize, FVector2D(460, 19), FMargin(.4f)),
		CapturePoint(320, 712), FVector2D(460, 19), 7);
	auto* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("TrainArmyQuantitySlider"));
	Slider->SetMinValue(0); Slider->SetMaxValue(1); Slider->SetIndentHandle(false);
	Slider->SetIsEnabled(bTrainArmyQuantityEditable);
	FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
	FSliderStyle Style;
	Style.SetNormalBarImage(Empty).SetHoveredBarImage(Empty).SetDisabledBarImage(Empty);
	const FSlateBrush Thumb = MakeAtlasBrush(LoadUiTexture(CommonAtlasPath), SpriteRect(679, 474, 20, 40), CommonAtlasSize, FVector2D(22, 41));
	Style.SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDisabledThumbImage(Thumb).SetBarThickness(19);
	Slider->SetWidgetStyle(Style);
	// Serialized direction1 + Arab flags34 are mirrored by Chinese Awake -> LTR.
	Slider->OnValueChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleTrainArmyQuantitySliderChanged);
	PlaceOnCanvas(ModeCanvas, Slider, CapturePoint(320, 701.5f), FVector2D(460, 40), 9);
	if (bTrainArmyQuantityEditable) UpdateTrainArmyQuantity(SelectedTrainCount);
}

void URokUnityFeatureScreenWidget::HandleTrainArmyQuantityTextChanged(const FText& Text)
{
	if (!bTrainArmyQuantitySync) UpdateTrainArmyQuantity(RokTrainArmyQuantity::FromText(Text.ToString(), TrainArmyBaseCapacity));
}

void URokUnityFeatureScreenWidget::HandleTrainArmyQuantitySliderChanged(float Value)
{
	if (bTrainArmyQuantitySync || !bTrainArmyQuantityEditable || bTrainArmyDetailVisible
		|| ActiveMode != ERokUnityFeatureScreenMode::TrainArmy) return;
	UpdateTrainArmyQuantity(RokTrainArmyQuantity::FromSlider(Value, TrainArmyBaseCapacity),
		RokTrainArmyQuantity::SliderPositionAfterDrag(Value, TrainArmyBaseCapacity));
	const double Now = FPlatformTime::Seconds();
	if (Now - TrainArmySliderSoundTime > 0.02)
	{
		TrainArmySliderSoundTime = Now;
		PlaySourceScrollFeedback();
	}
}

void URokUnityFeatureScreenWidget::UpdateTrainArmyQuantity(int32 Count, float SliderPosition)
{
	if (bTrainArmyQuantitySync || !bTrainArmyQuantityEditable || bTrainArmyDetailVisible
		|| TrainArmyInstantConfirmation || ActiveMode != ERokUnityFeatureScreenMode::TrainArmy) return;
	TGuardValue<bool> Guard(bTrainArmyQuantitySync, true);
	SelectedTrainCount = FMath::Clamp(Count, 1, FMath::Max(1, TrainArmyBaseCapacity));
	if (auto* Input = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("TrainArmyQuantityInput"))))
		Input->SetText(FText::FromString(FString::FromInt(SelectedTrainCount)));
	const float Fraction = SliderPosition >= 0 ? SliderPosition
		: static_cast<float>(SelectedTrainCount) / FMath::Max(1, TrainArmyBaseCapacity);
	if (auto* Slider = Cast<USlider>(GetWidgetFromName(TEXT("TrainArmyQuantitySlider")))) Slider->SetValue(Fraction);
	if (auto* Fill = GetWidgetFromName(TEXT("TrainArmyQuantitySliderFill")))
	{
		Fill->SetRenderTransformPivot(FVector2D(0, .5f)); Fill->SetRenderScale(FVector2D(Fraction, 1));
	}
	if (auto* Quantity = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainArmyQuantity"))))
		Quantity->SetText(FText::AsNumber(SelectedTrainCount));
	auto* Catalog = GetGameplayCatalogSubsystem();
	const auto Troops = Catalog ? Catalog->GetTroopsForArmyType(SelectedTrainArmyType) : TArray<FRokUnityCatalogTroop>();
	const auto* Troop = Troops.FindByPredicate([this](const auto& Row) { return Row.Level == SelectedTrainTier; });
	auto* Cost = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainArmyCostAndTime")));
	if (Troop && Cost)
	{
		int32 BootstrapOwned = 0;
		if (auto* Runtime = GetRuntimeStateSubsystem()) for (const auto& Stack : Runtime->GetStarterTroopStacks())
			if (Stack.TroopId == Troop->Id) BootstrapOwned += Stack.Count;
		const int32 Seconds = static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(Troop->TrainSeconds) * SelectedTrainCount, 0, MAX_int32));
		Cost->SetText(FText::Format(NSLOCTEXT("RokTrainArmy", "CostTimeFormat",
			"总消耗：{0}    训练时间：{1}\n本地启动快照拥有：{2}（仅迁移验收，不是服务器库存）"),
			FormatBuildCost(ScaleResourceCost(Troop->TrainCost, SelectedTrainCount)), FormatBuildDuration(Seconds), FText::AsNumber(BootstrapOwned)));
	}
	UpdateTrainResourceCost();
}

void URokUnityFeatureScreenWidget::BuildTrainResourceCost()
{
	// Keep the previous diagnostic text available to existing audits, without drawing duplicate costs.
	if (auto* Legacy = GetWidgetFromName(TEXT("TrainArmyCostAndTime"))) Legacy->SetVisibility(ESlateVisibility::Hidden);
	auto* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TrainResourceCostRoot"));
	PlaceOnCanvas(ModeCanvas, Root, CapturePoint(425, 505), FVector2D(750, 85), 8);
	const FIntRect Icons[] = { FoodRect, WoodRect, SpriteRect(947, 431, 80, 80), SpriteRect(865, 431, 80, 80) };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("TrainResourceCostCell_%d"), Index));
		PlaceOnCanvas(Root, Cell, FVector2D::ZeroVector, FVector2D(137, 40), 1);
		PlaceOnCanvas(Cell, MakeAtlasImage(*FString::Printf(TEXT("TrainResourceCostIcon_%d"), Index), CommonAtlasPath,
			Icons[Index], CommonAtlasSize, FVector2D(42)), FVector2D(-1, -1), FVector2D(42), 1);
		auto* Text = MakeScreenText(*FString::Printf(TEXT("TrainResourceCostAmount_%d"), Index), 15,
			FLinearColor(.8509804f, .83137256f, .7294118f, 1), true);
		Text->SetShadowOffset(FVector2D::ZeroVector);
		PlaceOnCanvas(Cell, Text, FVector2D(43.36f, 5), FVector2D(75.74f, 30), 2);
	}
	auto* Time = MakeScreenText(TEXT("TrainResourceBaseTime"), 15, FLinearColor(.82f, .82f, .78f, 1));
	Time->SetShadowOffset(FVector2D::ZeroVector);
	PlaceOnCanvas(Root, Time, FVector2D(0, 48), FVector2D(750, 30), 2);
	UpdateTrainResourceCost();
}

void URokUnityFeatureScreenWidget::UpdateTrainResourceCost()
{
	const FName CostRootName(TEXT("TrainResourceCostRoot"));
	UWidget* CostRoot = GetWidgetFromName(CostRootName);
	if (!CostRoot)
	{
		return;
	}
	auto* Catalog = GetGameplayCatalogSubsystem();
	const auto Troops = Catalog ? Catalog->GetTroopsForArmyType(SelectedTrainArmyType) : TArray<FRokUnityCatalogTroop>();
	const auto* Troop = Troops.FindByPredicate([this](const auto& Row) { return Row.Level == SelectedTrainTier; });
	FRokUnityCatalogTroop ServerTroop;
	const auto* Instance = GetGameInstance();
	const auto* Server = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	if (Server && Server->GetTrainingTroop(SelectedTrainArmyType, SelectedTrainTier, ServerTroop)) Troop = &ServerTroop;
	if (!Troop) return;
	const int32 Base[] = { Troop->TrainCost.Food, Troop->TrainCost.Wood, Troop->TrainCost.Stone, Troop->TrainCost.Gold };
	int32 VisibleCount = 0;
	for (int32 Value : Base) if (Value > 0) ++VisibleCount;
	const bool bWidgetBlueprint = ActiveModePage && ActiveModePage->Get<UWidget>(CostRootName) == CostRoot;
	const URokUiPageWidget* Layout = bWidgetBlueprint ? TrainResourceCostLayout.Get() : nullptr;
	TArray<const UCanvasPanelSlot*, TInlineAllocator<4>> Positions;
	if (Layout)
	{
		for (int32 Index = 0; Index < VisibleCount; ++Index)
		{
			const FName MarkerName(*FString::Printf(TEXT("TrainCostSlot_%d_%d"), VisibleCount, Index));
			const UWidget* Marker = Layout->Get<UWidget>(MarkerName);
			const auto* Slot = Marker ? Cast<UCanvasPanelSlot>(Marker->Slot) : nullptr;
			if (!Slot)
			{
				Positions.Reset();
				break;
			}
			Positions.Add(Slot);
		}
	}
	// The native parameter layout reparents the cost root into its animated panel.
	const bool bParameterLayout = CostRoot->GetParent() != ModeCanvas.Get();
	int32 VisibleIndex = 0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Cell = GetWidgetFromName(*FString::Printf(TEXT("TrainResourceCostCell_%d"), Index));
		if (!Cell) continue;
		Cell->SetVisibility(Base[Index] > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (Base[Index] <= 0) continue;
		if (auto* CellSlot = Cast<UCanvasPanelSlot>(Cell->Slot))
		{
			if (Positions.IsValidIndex(VisibleIndex))
			{
				const auto* Position = Positions[VisibleIndex];
				CellSlot->SetAnchors(Position->GetAnchors());
				CellSlot->SetAlignment(Position->GetAlignment());
				CellSlot->SetPosition(Position->GetPosition());
			}
			else
			{
				CellSlot->SetAnchors(FAnchors(0, 0));
				CellSlot->SetAlignment(FVector2D::ZeroVector);
				CellSlot->SetPosition(FVector2D((bParameterLayout ? 0 : 101) + RokTrainResourceCost::CellX(VisibleCount, VisibleIndex),
					bParameterLayout ? 13.95 : 0));
			}
			++VisibleIndex;
		}
		if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("TrainResourceCostAmount_%d"), Index))))
			Text->SetText(FText::FromString(RokTrainResourceCost::Format(static_cast<int64>(Base[Index]) * SelectedTrainCount)));
	}
	if (auto* Time = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainResourceBaseTime"))))
	{
		const int64 Seconds = Server ? Server->GetTrainingSeconds(*Troop, SelectedTrainCount)
			: int64(Troop->TrainSeconds) * SelectedTrainCount;
		Time->SetText(FText::FromString(FString::Printf(TEXT("%02lld:%02lld:%02lld"), Seconds / 3600, Seconds / 60 % 60, Seconds % 60)));
	}
	RefreshTrainArmyAction();
}

void URokUnityFeatureScreenWidget::BuildTrainArmySharedDisplay(int32 TroopId)
{
	// ShowAttrDetail / OnDetailReturn keep pl_left and the window frame alive.
	FRokTrainArmyDetail Detail; FString Error;
	if (!RokTrainArmyDetail::Load(TroopId, Detail, Error)) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return; }
	const TCHAR* TroopAtlas = *Detail.Attributes[0].Texture;
	const auto TroopAtlasSize = Detail.Attributes[0].AtlasSize;
	UCanvasPanel* ModelParent = ModeCanvas;
	if (ActiveModePage)
	{
		auto* Backing = ActiveModePage->Get<UCanvasPanel>(TEXT("TrainParameterLeftBacking"));
		auto* Art = ActiveModePage->Get<UImage>(TEXT("TrainParameterLeftArt"));
		auto* Host = ActiveModePage->Get<UCanvasPanel>(TEXT("TrainArmyModelHost"));
		if (Backing && Art && Host)
		{
			Backing->SetVisibility(ESlateVisibility::HitTestInvisible);
			UTexture2D* Texture = LoadUiTexture(TroopAtlas);
			Art->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (Texture)
			{
				Art->SetBrush(MakeAtlasBrush(Texture, SpriteRect(1, 1392, 447, 655), TroopAtlasSize, FVector2D(453.5, 656)));
			}
			ModelParent = Host;
		}
	}
	if (ModelParent == ModeCanvas)
	{
		// Source left art stays fixed while the pl_spine mesh keeps its own idle clock.
		auto* LeftBack = MakeAtlasBorder(TEXT("TrainParameterLeftBacking"), CommonAtlasPath, SpriteRect(1003, 932, 400, 42),
			CommonAtlasSize, FVector2D(464.9, 656), FMargin(182.f / 400, 13.f / 42, 115.f / 400, 12.f / 42));
		PlaceOnCanvas(ModeCanvas, LeftBack, FVector2D(175.95, 99.9), FVector2D(464.9, 656), 3);
		LeftBack->SetRenderTransformPivot(FVector2D(.5, .5)); LeftBack->SetRenderScale(FVector2D(-1, 1));
		auto* LeftImage = MakeAtlasImage(TEXT("TrainParameterLeftArt"), TroopAtlas, SpriteRect(1, 1392, 447, 655), TroopAtlasSize, FVector2D(453.5, 656));
		PlaceOnCanvas(ModeCanvas, LeftImage, FVector2D(179.05, 99.8), FVector2D(453.5, 656), 4);
		LeftImage->SetRenderTransformPivot(FVector2D(.5, .5)); LeftImage->SetRenderScale(FVector2D(-1, 1));
	}
	if (!TrainArmyAnimatedModel)
		TrainArmyAnimatedModel = WidgetTree->ConstructWidget<URokTrainArmyMeshWidget>(URokTrainArmyMeshWidget::StaticClass(), TEXT("TrainArmyAnimatedModel"));
	if (TrainArmyAnimatedModel->LoadArms(TroopId))
	{
		TrainArmyAnimatedModel->SetVisibility(ESlateVisibility::HitTestInvisible);
		const auto* HostSlot = ModelParent != ModeCanvas ? Cast<UCanvasPanelSlot>(ModelParent->Slot) : nullptr;
		const FVector2D Extent = HostSlot ? HostSlot->GetSize() : FVector2D(1440, 810);
		PlaceOnCanvas(ModelParent, TrainArmyAnimatedModel, FVector2D::ZeroVector, Extent, 4);
		return;
	}
	// Missing source is a visible diagnostic, never a fabricated animated model.
	FIntRect PortraitRect;
	const bool bHasPortrait = ResolveTroopHeadRect(SelectedTrainArmyType, SelectedTrainTier, PortraitRect);
	if (TryBuildTrainArmyUnavailableState(TEXT("ModelMissing"), ModelParent, bHasPortrait ? &PortraitRect : nullptr))
	{
		return;
	}
	if (bHasPortrait)
	{
		PlaceOnCanvas(ModelParent, MakeAtlasImage(bTrainArmyDetailVisible ? TEXT("TrainArmyDetailPortrait") : TEXT("TrainArmySelectedIcon"),
			TroopHeadAtlasPath, PortraitRect, TroopHeadAtlasSize, FVector2D(280)), FVector2D(270, 250), FVector2D(280), 6);
	}
	auto* Preview = MakeScreenText(TEXT("TrainParameterStaticPreview"), 15, FLinearColor::White, false, ETextJustify::Center);
	Preview->SetText(FText::FromString(TEXT("模型加载失败 · 静态诊断预览"))); Preview->SetShadowOffset(FVector2D::ZeroVector);
	PlaceOnCanvas(ModelParent, Preview, FVector2D(270, 540), FVector2D(280, 30), 6);
}

void URokUnityFeatureScreenWidget::ApplyTrainParameterLayout(int32 TroopId)
{
	using namespace RokTrainParameterLayout;
	FRokTrainArmyDetail Detail; FString Error;
	if (!RokTrainArmyDetail::Load(TroopId, Detail, Error)) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return; }
	auto Move = [this](const FName Name, FVector2D Position, FVector2D Size)
	{
		if (auto* Widget = GetWidgetFromName(Name)) if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{ CanvasSlot->SetPosition(Position); CanvasSlot->SetSize(Size); }
	};
	auto Hide = [this](const FName Name) { if (auto* Widget = GetWidgetFromName(Name)) Widget->SetVisibility(ESlateVisibility::Collapsed); };
	for (const FName Name : { FName(TEXT("TrainArmyContentShade")), FName(TEXT("TrainArmySelectedShade")),
		FName(TEXT("TrainArmyQuantityShade")), FName(TEXT("TrainArmyBuildingContext")), FName(TEXT("TrainArmyMinus")),
		FName(TEXT("TrainArmyPlus")), FName(TEXT("TrainArmyMax")) }) Hide(Name);
	const TCHAR* TroopAtlas = *Detail.Attributes[0].Texture;
	const auto TroopAtlasSize = Detail.Attributes[0].AtlasSize;
	const FMargin PaperMargin(16.f / 63, 13.f / 55, 15.f / 63, 13.f / 55);
	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("TrainArmyParameterHeadBackground"), CommonAtlasPath, SpriteRect(436, 165, 63, 55),
		CommonAtlasSize, FVector2D(584, 114), PaperMargin), FVector2D(659.9, 155.4), FVector2D(584, 114), 3);
	auto* CostBackground = MakeAtlasBorder(TEXT("TrainArmyParameterCostBackground"), CommonAtlasPath, SpriteRect(436, 165, 63, 55),
		CommonAtlasSize, FVector2D(590, 160), PaperMargin);
	PlaceOnCanvas(ModeCanvas, CostBackground, FVector2D(656.9, 398.4), FVector2D(590, 160), 3);
	auto* QuantityBand = MakeAtlasBorder(TEXT("TrainArmyParameterQuantityBand"), CommonAtlasPath, SpriteRect(436, 165, 63, 55),
		CommonAtlasSize, FVector2D(548, 39.93), PaperMargin);
	PlaceOnCanvas(ModeCanvas, QuantityBand, FVector2D(677.9, 431.035), FVector2D(548, 39.93), 4);
	if (!bTrainArmyQuantityEditable) { CostBackground->SetVisibility(ESlateVisibility::Collapsed); QuantityBand->SetVisibility(ESlateVisibility::Collapsed); }
	Move(TEXT("TrainArmyDetailButton"), FVector2D(581, 124.9), FVector2D(60, 60));
	Hide(TEXT("TrainArmyDetailButtonBackground")); Hide(TEXT("TrainArmyDetailButtonLabel"));
	if (auto* Content = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("TrainArmyDetailButtonContent"))))
		PlaceOnCanvas(Content, MakeAtlasImage(TEXT("TrainParameterDetailIcon"), CommonAtlasPath, SpriteRect(782, 237, 37, 37), CommonAtlasSize, FVector2D(38)), FVector2D(11, 11), FVector2D(38), 1);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FName IconName(*FString::Printf(TEXT("TrainArmyTierIcon_%d"), Index));
		Move(IconName, HeadIcon(Index), FVector2D(126));
		Hide(*FString::Printf(TEXT("TrainArmyTierQuality_%d"), Index));
		auto* Select = MakeAtlasImage(*FString::Printf(TEXT("TrainParameterHeadSelect_%d"), Index), TroopAtlas, SpriteRect(129, 742, 126, 126), TroopAtlasSize, FVector2D(126));
		PlaceOnCanvas(ModeCanvas, Select, HeadIcon(Index) + FVector2D(0, 2), FVector2D(126), 8);
		if (SelectedTrainTier != Index + 1) Select->SetVisibility(ESlateVisibility::Collapsed);
		Move(*FString::Printf(TEXT("TrainArmyTier_%d"), Index), HeadHit(Index), FVector2D(105.4, 105.1));
		const FName LabelName(*FString::Printf(TEXT("TrainArmyTierLabel_%d"), Index));
		Move(LabelName, HeadOrigin(Index) + FVector2D(-21, 143.7), FVector2D(160, 30));
		if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(LabelName)))
		{
			Label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16));
			Label->SetShadowOffset(FVector2D::ZeroVector);
		}
	}
	Move(TEXT("TrainArmySelectedName"), FVector2D(712.36, 286.47), FVector2D(528.35, 30));
	const FIntRect TypeRects[] = { SpriteRect(450, 1906, 38, 38), SpriteRect(87, 413, 38, 38),
		SpriteRect(44, 369, 38, 38), SpriteRect(1, 92, 38, 38) };
	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("TrainArmyParameterTypeIcon"), TroopAtlas, TypeRects[SelectedTrainArmyType - 1],
		TroopAtlasSize, FVector2D(38)), FVector2D(668.9, 281.47), FVector2D(38), 7);
	if (auto* Name = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainArmySelectedName"))))
	{
		Name->SetText(FText::FromString(Detail.Name)); auto Font = Name->GetFont(); Font.Size = 19.5f; Name->SetFont(Font);
		Name->SetColorAndOpacity(FLinearColor(.8862745f, .81960785f, .60784316f, 1)); Name->SetShadowOffset(FVector2D::ZeroVector);
	}
	Move(TEXT("TrainArmySelectedStats"), FVector2D(668.395, 328.6), FVector2D(567.01, 60.9));
	if (auto* Description = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainArmySelectedStats"))))
	{
		Description->SetText(FText::FromString(Detail.Description)); auto Font = Description->GetFont(); Font.Size = 13.5f; Description->SetFont(Font);
		Description->SetColorAndOpacity(FLinearColor::White); Description->SetAutoWrapText(true); Description->SetWrapTextAt(567.01f);
		Description->SetShadowOffset(FVector2D::ZeroVector); Description->SetClipping(EWidgetClipping::ClipToBounds);
	}
	Move(TEXT("TrainArmyQuantityInput"), InputPosition(), FVector2D(136.53, 40));
	if (auto* Input = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("TrainArmyQuantityInput"))))
	{
		FEditableTextBoxStyle Style = Input->GetWidgetStyle();
		Style.TextStyle.Font.Size = 15;
		Input->SetWidgetStyle(Style);
		// UE 5.8 Slate retains the supplied style pointer once the box is mounted.
		Input->SetWidgetStyle(Input->GetWidgetStyle());
	}
	Move(TEXT("TrainArmyQuantity"), InputPosition(), FVector2D(136.53, 40));
	if (auto* Quantity = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainArmyQuantity"))))
	{
		Quantity->SetText(FText::AsNumber(SelectedTrainCount));
		Quantity->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15));
		Quantity->SetColorAndOpacity(FLinearColor(.886f, .820f, .608f));
		Quantity->SetShadowOffset(FVector2D::ZeroVector);
		Quantity->RemoveFromParent();
		auto* Region = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TrainArmyQuantityRegion"));
		FSlateBrush Empty; Empty.DrawAs = ESlateBrushDrawType::NoDrawType; Region->SetBrush(Empty); Region->SetPadding(FMargin(0));
		Region->SetHorizontalAlignment(HAlign_Fill); Region->SetVerticalAlignment(VAlign_Center);
		Region->SetVisibility(bTrainArmyQuantityEditable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Region->SetContent(Quantity); PlaceOnCanvas(ModeCanvas, Region, InputPosition(), FVector2D(136.53, 40), 7);
	}
	Move(TEXT("TrainArmyQuantitySlider"), SliderPosition(), FVector2D(460, 41));
	Move(TEXT("TrainArmyQuantitySliderBackground"), SliderPosition() + FVector2D(0, 9), FVector2D(460, 23));
	Move(TEXT("TrainArmyQuantitySliderFill"), SliderPosition() + FVector2D(0, 11), FVector2D(460, 19));
	Move(TEXT("TrainResourceCostRoot"), CostPosition(), FVector2D(548, 105));
	Move(TEXT("TrainArmyStart"), TrainPosition(), FVector2D(274, 84));
	Hide(TEXT("TrainArmyStartBackground"));
	const FVector2D ActionSize(274, 84);
	const FLinearColor ActionInk(FColor(25, 28, 25));
	if (auto* StartContent = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("TrainArmyStartContent"))))
	{
		PlaceOnCanvas(StartContent, MakeAtlasImage(TEXT("TrainParameterStartBlue"), CommonAtlasPath,
			LargeBlueButtonRect, CommonAtlasSize, ActionSize), FVector2D::ZeroVector, ActionSize, 0);
		if (bTrainCanStart)
		{
			// UI_Model_DoubleLineButton_Blue/img_icon2 uses icon_com_1001.
			PlaceOnCanvas(StartContent, MakeAtlasImage(TEXT("TrainArmyDurationIcon"), CommonAtlasPath,
				SpriteRect(340, 32, 42, 46), CommonAtlasSize, FVector2D(21, 23)), FVector2D(87, 45), FVector2D(21, 23), 2);
			if (auto* Time = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainResourceBaseTime"))))
			{
				Time->RemoveFromParent();
				Time->SetColorAndOpacity(FLinearColor::White);
				PlaceOnCanvas(StartContent, Time, FVector2D(111, 48), FVector2D(125, 24), 2);
			}
		}
	}
	Move(TEXT("TrainArmyStartLabel"), FVector2D(0, bTrainCanStart ? 12 : 23), FVector2D(274, 38));
	if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainArmyStartLabel"))))
	{
		Label->SetColorAndOpacity(ActionInk);
		auto Font = Label->GetFont(); Font.Size = 23; Label->SetFont(Font); Label->SetShadowOffset(FVector2D::ZeroVector);
	}
	if (bTrainCanStart)
	{
		auto* Instant = MakeActionButton(TEXT("TrainArmyInstant"), 1941, CommonAtlasPath,
			SpriteRect(1766, 586, 274, 84), CommonAtlasSize, ActionSize,
			NSLOCTEXT("RokTrainArmy", "InstantAction", "立即完成"), 23, ActionInk);
		PlaceOnCanvas(ModeCanvas, Instant, TrainPosition() - FVector2D(317, 0), ActionSize, 9);
		Move(TEXT("TrainArmyInstantLabel"), FVector2D(0, 12), FVector2D(274, 38));
		if (auto* Content = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("TrainArmyInstantContent"))))
		{
			PlaceOnCanvas(Content, MakeAtlasImage(TEXT("TrainArmyInstantGem"), CommonAtlasPath, GemRect,
				CommonAtlasSize, FVector2D(22)), FVector2D(115, 46), FVector2D(22), 2);
			auto* Price = MakeScreenText(TEXT("TrainArmyInstantPrice"), 16, FLinearColor::White, true);
			Price->SetShadowOffset(FVector2D::ZeroVector);
			PlaceOnCanvas(Content, Price, FVector2D(139, 48), FVector2D(108, 24), 2);
		}
	}
	Move(TEXT("TrainArmyStudyStatus"), FVector2D(677.9, 438.4), FVector2D(548, 70));
	if (!bTrainArmyQuantityEditable)
	{
		for (const FName Name : {FName(TEXT("TrainArmyQuantityInput")), FName(TEXT("TrainArmyQuantitySlider")),
			FName(TEXT("TrainArmyQuantitySliderBackground")), FName(TEXT("TrainArmyQuantitySliderFill")),
			FName(TEXT("TrainResourceCostRoot"))})
			Hide(Name);
		Move(TEXT("TrainArmyServerQueueStatus"), FVector2D(678, 440), FVector2D(548, 39));
		Move(TEXT("TrainArmyServerQueueProgress"), FVector2D(678, 484), FVector2D(548, 12));
	}
	// Only pl_train plays Show on detail return. Frame, left art and detail button stay still.
	auto* Right = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TrainParameterRoot"));
	PlaceOnCanvas(ModeCanvas, Right, RightOrigin(), RightSize(), 5);
	TArray<UWidget*> Children;
	for (int32 Index = 0; Index < ModeCanvas->GetChildrenCount(); ++Index) Children.Add(ModeCanvas->GetChildAt(Index));
	for (auto* Child : Children)
	{
		const FString Name = Child->GetName();
		const bool bTrain = Name.StartsWith(TEXT("TrainArmy")) || Name.StartsWith(TEXT("TrainResource")) || Name.StartsWith(TEXT("TrainParameterHead"));
		if (!bTrain || Name == TEXT("TrainArmySelectedIcon") || Name == TEXT("TrainArmyAnimatedModel") || Name == TEXT("TrainArmyDetailButton")
			|| Name == TEXT("TrainArmyContentShade") || Name == TEXT("TrainArmyBuildingContext")) continue;
		if (auto* OldSlot = Cast<UCanvasPanelSlot>(Child->Slot))
		{
			const auto Position = OldSlot->GetPosition() - RightOrigin(), Size = OldSlot->GetSize(); const int32 Z = OldSlot->GetZOrder();
			Child->RemoveFromParent(); PlaceOnCanvas(Right, Child, Position, Size, Z);
		}
	}
	TrainArmyAnimatedChildren.Reset(); TrainArmyAnimatedChildren.Add(Right);
	UpdateTrainResourceCost();
}

bool URokUnityFeatureScreenWidget::BuildTrainArmyParameterPage(const FRokUnityCatalogTroop& SelectedTroop,
	const TArray<FRokUnityCatalogTroop>& Troops, const FRokServerArmySnapshot& ArmySnapshot,
	const URokServerArmySubsystem* ServerArmy, const FRokServerTrainingQueue* ServerQueue,
	const FText& ContextText, bool bTierUnlocked, bool bIdle)
{
	UClass* TierClass = ResolveFeaturePageClass(TEXT("TrainArmyTierCard"));
	if (!TierClass)
	{
		return false;
	}
	FRokTrainArmyDetail Detail;
	FString DetailError;
	if (!RokTrainArmyDetail::Load(SelectedTroop.Id, Detail, DetailError) || Detail.Attributes.IsEmpty())
	{
		return false;
	}
	auto* Page = MountModePage(TEXT("TrainArmyParameter"), false, 5);
	if (!Page)
	{
		return false;
	}
	auto* Right = Page->Get<UCanvasPanel>(TEXT("TrainParameterRoot"));
	if (!Right)
	{
		Page->RemoveFromParent();
		ActiveModePage = nullptr;
		return false;
	}
	if (!TrainResourceCostLayout)
	{
		if (UClass* LayoutClass = ResolveFeaturePageClass(TEXT("TrainResourceCostLayout")))
		{
			TrainResourceCostLayout = CreateWidget<URokUiPageWidget>(this, LayoutClass);
		}
	}
	UClass* ActionPresentationClass = ResolveFeaturePageClass(TEXT("TrainArmyActionPresentation"));
	const auto* ActionPresentation = ActionPresentationClass
		? CreateWidget<URokUiPageWidget>(this, ActionPresentationClass) : nullptr;

	const auto SetText = [](const URokUiPageWidget& Owner, FName Name, const FText& Text)
	{
		if (auto* Label = Owner.Get<UTextBlock>(Name)) Label->SetText(Text);
	};
	const auto SetShown = [Page](FName Name, bool bVisible, ESlateVisibility Shown = ESlateVisibility::HitTestInvisible)
	{
		if (auto* Widget = Page->Get<UWidget>(Name))
		{
			Widget->SetVisibility(bVisible ? Shown : ESlateVisibility::Collapsed);
		}
	};
	UMaterialInstanceDynamic* GrayPortrait = nullptr;
	auto* HeadTexture = LoadUiTexture(TroopHeadAtlasPath);
	if (auto* Gray = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/RokPrototype/UI/Research/M_RokResearchGrayUI.M_RokResearchGrayUI")))
	{
		GrayPortrait = UMaterialInstanceDynamic::Create(Gray, this);
		GrayPortrait->SetTextureParameterValue(TEXT("SourceAtlas"), HeadTexture);
	}
	for (int32 Index = 0; Index < 5; ++Index)
	{
		auto* CardSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("TrainArmyTierSlot_%d"), Index));
		auto* Card = MountPageItem(CardSlot, TierClass, TEXT("TrainArmy"), Index);
		if (!Card) continue;
		const int32 Tier = Index + 1;
		const auto* TierTroop = Troops.FindByPredicate([Tier](const auto& Troop) { return Troop.Level == Tier; });
		const bool bUnlocked = TierTroop && (TierTroop->StudyId == 0
			|| (ServerArmy && ServerArmy->IsTrainingTierUnlocked(TierTroop->StudyId)));
		if (auto* Portrait = Card->Get<UImage>(TEXT("TierIcon")))
		{
			FIntRect Rect;
			const bool bHasArt = HeadTexture && ResolveTroopHeadRect(SelectedTrainArmyType, Tier, Rect);
			Card->SetPresent(Portrait, bHasArt);
			if (bHasArt)
			{
				FSlateBrush Brush = MakeAtlasBrush(HeadTexture, Rect, TroopHeadAtlasSize, FVector2D(116));
				if (!bUnlocked && GrayPortrait) Brush.SetResourceObject(GrayPortrait);
				Portrait->SetBrush(Brush);
			}
		}
		int64 Owned = 0;
		for (const auto& Stack : ArmySnapshot.CitySoldiers)
		{
			if (Stack.FamilyType == SelectedTrainArmyType && Stack.Tier == Tier) Owned += Stack.Count;
		}
		if (auto* Label = Card->Get<UTextBlock>(TEXT("TierLabel")))
		{
			Label->SetText(ArmySnapshot.bCityKnown ? FText::AsNumber(Owned) : FText::FromString(TEXT("—")));
			Label->SetColorAndOpacity(FeaturePresentationColor(ActionPresentation,
				bUnlocked ? TEXT("TrainTierUnlocked") : TEXT("TrainTierLocked"),
				bUnlocked ? FLinearColor::White : FLinearColor(.35f, .35f, .34f)));
		}
		Card->SetShown(Card->Get<UImage>(TEXT("TrainParameterHeadSelect")), Tier == SelectedTrainTier);
		BindPageAction(*Card, TEXT("Tier"), 1900 + Index);
	}

	SetText(*Page, TEXT("TrainArmyBuildingContext"), ContextText);
	SetText(*Page, TEXT("TrainArmySelectedName"), FText::FromString(Detail.Name));
	SetText(*Page, TEXT("TrainArmySelectedStats"), FText::FromString(Detail.Description));
	if (auto* TypeIcon = Page->Get<UImage>(TEXT("TrainArmyParameterTypeIcon")))
	{
		const FIntRect Types[] = {SpriteRect(450, 1906, 38, 38), SpriteRect(87, 413, 38, 38),
			SpriteRect(44, 369, 38, 38), SpriteRect(1, 92, 38, 38)};
		const auto& Source = Detail.Attributes[0];
		auto* Texture = LoadUiTexture(*Source.Texture);
		TypeIcon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Texture) TypeIcon->SetBrush(MakeAtlasBrush(Texture, Types[SelectedTrainArmyType - 1], Source.AtlasSize, FVector2D(38)));
	}

	const bool bLocked = !bTierUnlocked && !bTrainActionReceives;
	SetShown(TEXT("TrainArmyQuantity"), !bLocked);
	SetShown(TEXT("TrainArmyQuantityRegion"), bTrainArmyQuantityEditable);
	SetShown(TEXT("TrainArmyQuantityInput"), bTrainArmyQuantityEditable, ESlateVisibility::Visible);
	SetShown(TEXT("TrainArmyQuantitySlider"), bTrainArmyQuantityEditable, ESlateVisibility::Visible);
	for (const FName Name : {FName(TEXT("TrainArmyQuantitySliderBackground")), FName(TEXT("TrainArmyQuantitySliderFill")),
		FName(TEXT("TrainArmyParameterCostBackground")), FName(TEXT("TrainArmyParameterQuantityBand"))})
	{
		SetShown(Name, bTrainArmyQuantityEditable);
	}
	SetShown(TEXT("TrainResourceCostRoot"), bTrainArmyQuantityEditable, ESlateVisibility::SelfHitTestInvisible);
	SetShown(TEXT("TrainArmyStart"), !bLocked, ESlateVisibility::Visible);
	SetShown(TEXT("TrainArmyDurationIcon"), bTrainCanStart);
	SetShown(TEXT("TrainResourceBaseTime"), bTrainCanStart);
	Page->SetPresent(Page->Get<UWidget>(TEXT("TrainArmyInstant")), bTrainCanStart);
	BindPageAction(*Page, TEXT("TrainArmyStart"), 1940);
	BindPageAction(*Page, TEXT("TrainArmyDetailButton"), 1950);
	if (bTrainCanStart) BindPageAction(*Page, TEXT("TrainArmyInstant"), 1941);
	if (auto* Label = Page->Get<UTextBlock>(TEXT("TrainArmyStartLabel")))
	{
		if (auto* LabelSlot = Cast<UCanvasPanelSlot>(Label->Slot))
		{
			const auto* Marker = ActionPresentation ? ActionPresentation->Get<UWidget>(
				bTrainCanStart ? TEXT("TrainStartLabelReady") : TEXT("TrainStartLabelOther")) : nullptr;
			const auto* MarkerSlot = Marker ? Cast<UCanvasPanelSlot>(Marker->Slot) : nullptr;
			const double Y = MarkerSlot ? MarkerSlot->GetPosition().Y : (bTrainCanStart ? 12.0 : 23.0);
			LabelSlot->SetPosition(FVector2D(LabelSlot->GetPosition().X, Y));
		}
	}
	SetShown(TEXT("TrainArmyStudyStatus"), bLocked);
	SetText(*Page, TEXT("TrainArmyStudyStatus"), ArmySnapshot.bAvailable
		? NSLOCTEXT("RokTrainArmy", "StudyStatusRequired", "当前兵阶需要完成对应科技。可查看兵种详情。")
		: NSLOCTEXT("RokTrainArmy", "StudyStatusUnknown", "当前兵阶需要科技解锁，解锁状态待同步。可查看兵种详情。"));

	Page->SetPresent(Page->Get<UWidget>(TEXT("TrainArmyServerQueueStatus")), ArmySnapshot.bAvailable);
	Page->SetPresent(Page->Get<UWidget>(TEXT("TrainArmyServerQueueProgress")), ArmySnapshot.bAvailable);
	if (ArmySnapshot.bAvailable)
	{
		SetShown(TEXT("TrainArmyServerQueueStatus"), !bIdle);
		SetText(*Page, TEXT("TrainArmyServerQueueStatus"),
			DescribeServerTrainingQueue(ArmySnapshot, SelectedServerTrainBuildingIndex, SelectedTrainArmyType));
		if (auto* QueueProgress = Page->Get<UProgressBar>(TEXT("TrainArmyServerQueueProgress")))
		{
			const bool bTiming = ServerQueue && ServerQueue->bTimeKnown
				&& (ServerQueue->State == ERokServerTrainingState::Training || ServerQueue->State == ERokServerTrainingState::Upgrading);
			QueueProgress->SetPercent(bTiming ? FMath::Clamp(static_cast<float>(ServerQueue->Progress01), 0.0f, 1.0f) : 0.0f);
			QueueProgress->SetVisibility(bTiming ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	if (auto* Input = Page->Get<UEditableTextBox>(TEXT("TrainArmyQuantityInput")))
	{
		Input->SetText(FText::FromString(FString::FromInt(SelectedTrainCount)));
		Input->SetIsEnabled(bTrainArmyQuantityEditable);
		Input->OnTextChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleTrainArmyQuantityTextChanged);
	}
	if (auto* Slider = Page->Get<USlider>(TEXT("TrainArmyQuantitySlider")))
	{
		Slider->SetIsEnabled(bTrainArmyQuantityEditable);
		Slider->OnValueChanged.AddDynamic(this, &URokUnityFeatureScreenWidget::HandleTrainArmyQuantitySliderChanged);
	}
	SetText(*Page, TEXT("TrainArmyQuantity"), FText::AsNumber(SelectedTrainCount));
	if (bTrainArmyQuantityEditable) UpdateTrainArmyQuantity(SelectedTrainCount);
	UpdateTrainResourceCost();
	TrainArmyAnimatedChildren.Add(Right);
	return true;
}

void URokUnityFeatureScreenWidget::BuildTrainArmyDetail(int32 TroopId)
{
	FRokTrainArmyDetail Detail; FString Error;
	if (!RokTrainArmyDetail::Load(TroopId, Detail, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		if (TryBuildTrainArmyUnavailableState(TEXT("DetailMissing"), ModeCanvas))
		{
			return;
		}
		auto* Missing = MakeScreenText(TEXT("TrainArmyDetailMissing"), 22, FLinearColor::White);
		Missing->SetText(FText::FromString(TEXT("兵种详情配置未载入")));
		PlaceOnCanvas(ModeCanvas, Missing, CapturePoint(650, 300), FVector2D(540, 60), 10);
	}
	else if (BuildTrainArmyDetailPage(Detail))
	{
		return;
	}
	else
	{
		auto* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TrainArmyDetailRoot"));
		// Source detail rect is 624x642. The frame and portrait are not this Animator's children.
		PlaceOnCanvas(ModeCanvas, Panel, CapturePoint(618, 137), FVector2D(624, 642), 5);
		TrainArmyAnimatedChildren.Add(Panel);
		auto* Name = MakeScreenText(TEXT("TrainArmyDetailName"), 25, FLinearColor(0.98f, 0.91f, 0.56f, 1.0f), true);
		Name->SetText(FText::FromString(Detail.Name));
		PlaceOnCanvas(Panel, Name, FVector2D(57.05f, 53.8f), FVector2D(449.4f, 30), 1);
		// RS.ArmyTypeIcon keeps cavalry and archer sprite IDs in source order 1,3,2,4.
		const FIntRect TypeRects[] = { SpriteRect(450, 1906, 38, 38), SpriteRect(87, 413, 38, 38),
			SpriteRect(44, 369, 38, 38), SpriteRect(1, 92, 38, 38) };
		PlaceOnCanvas(Panel, MakeAtlasImage(TEXT("TrainArmyDetailTypeIcon"), *Detail.Attributes[0].Texture,
			TypeRects[SelectedTrainArmyType - 1], Detail.Attributes[0].AtlasSize, FVector2D(38)),
			FVector2D(23.05f, 49.9f), FVector2D(38), 2);
		// Unity Text uses pixels; Slate font sizes are points rendered at 96 DPI.
		auto* Description = MakeScreenText(TEXT("TrainArmyDetailDescription"), 15, FLinearColor::White);
		Description->SetText(FText::FromString(Detail.Description)); Description->SetAutoWrapText(true);
		Description->SetWrapTextAt(510.8f);
		Description->SetShadowOffset(FVector2D::ZeroVector);
		Description->SetClipping(EWidgetClipping::ClipToBounds);
		PlaceOnCanvas(Panel, Description, FVector2D(36.15f, 89.2f), FVector2D(510.8f, 74.1f), 1);
		for (int32 Index = 0; Index < Detail.Attributes.Num(); ++Index)
		{
			const auto& Attribute = Detail.Attributes[Index];
			const float Top = 321.0f - Attribute.UnityY - 20.0f;
			// ArabLayout Awake in the Chinese branch mirrors PosX and TextAlignX.
			auto* Label = MakeScreenText(*FString::Printf(TEXT("TrainArmyDetailLabel_%d"), Index), 23,
				FLinearColor::White, false, ETextJustify::Right);
			Label->SetText(FText::FromString(Attribute.Label));
			PlaceOnCanvas(Panel, Label, FVector2D(19.96f, Top), FVector2D(263.5f, 40), 2);
			auto* Value = MakeScreenText(*FString::Printf(TEXT("TrainArmyDetailAttribute_%d"), Index), 23, FLinearColor::White);
			Value->SetText(FText::FromString(Attribute.Value + (Attribute.bModifierUnknown ? TEXT(" + —") : TEXT(""))));
			PlaceOnCanvas(Panel, Value, FVector2D(339.83f, Top), FVector2D(266.8f, 40), 2);
			PlaceOnCanvas(Panel, MakeAtlasImage(*FString::Printf(TEXT("TrainArmyDetailAttrIcon_%d"), Index),
				*Attribute.Texture, Attribute.Rect, Attribute.AtlasSize, FVector2D(30)),
				FVector2D(297, Top + 8.8f), FVector2D(30), 3);
			PlaceOnCanvas(Panel, MakeActionButton(*FString::Printf(TEXT("TrainArmyDetailAttrButton_%d"), Index),
				1960 + Index, nullptr, FIntRect(), FIntPoint(1, 1), FVector2D(591, 40)),
				FVector2D(16.5f, Top), FVector2D(591, 40), 4);
		}
		auto* Unknown = MakeScreenText(TEXT("TrainArmyDetailModifierStatus"), 16, FLinearColor(0.8f, 0.8f, 0.75f, 1.0f));
		Unknown->SetText(FText::FromString(TEXT("—：个人属性加成待同步")));
		PlaceOnCanvas(Panel, Unknown, FVector2D(30, 485), FVector2D(560, 24), 2);
		// Grid cell100 + spacing40; Chinese StartCorner0 retains siblings Skill_3,2,1.
		for (int32 Index = 0; Index < Detail.Skills.Num(); ++Index)
		{
			const auto& Skill = Detail.Skills[Index];
			const FVector2D SkillPosition(RokTrainArmyDetail::SkillCellX(Detail.Skills.Num(), Index) + 8, 511.25f);
			PlaceOnCanvas(Panel, MakeActionButton(*FString::Printf(TEXT("TrainArmyDetailSkillButton_%d"), Index),
				1970 + Index, CommonAtlasPath, SpriteRect(1, 151, 134, 134), CommonAtlasSize, FVector2D(84)),
				SkillPosition, FVector2D(84), 3);
			PlaceOnCanvas(Panel, MakeAtlasImage(*FString::Printf(TEXT("TrainArmyDetailSkillIcon_%d"), Index),
				*Skill.Texture, Skill.Rect, Skill.AtlasSize, FVector2D(72)), SkillPosition + FVector2D(6), FVector2D(72), 4);
		}
		AdvanceTrainArmyDetailAnimation(0.0f);
	}
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("TrainArmyDetailBackButton"), 1951, CommonAtlasPath,
		MediumGoldButtonRect, CommonAtlasSize, FVector2D(130, 48), FText::FromString(TEXT("返回")), 23),
		CapturePoint(198, 735), FVector2D(130, 48), 10);
}

bool URokUnityFeatureScreenWidget::BuildTrainArmyDetailPage(const FRokTrainArmyDetail& Detail)
{
	UClass* AttributeClass = ResolveFeaturePageClass(TEXT("TrainArmyDetailAttribute"));
	UClass* SkillClass = ResolveFeaturePageClass(TEXT("TrainArmyDetailSkill"));
	const int32 SkillCount = Detail.Skills.Num();
	auto* Page = AttributeClass && SkillClass && SkillCount <= 3 && Detail.Attributes.Num() <= 7
		? MountModePage(TEXT("TrainArmyDetail"), false, 5) : nullptr;
	if (!Page)
	{
		return false;
	}
	auto* Panel = Page->Get<UCanvasPanel>(TEXT("TrainArmyDetailRoot"));
	if (!Panel)
	{
		Page->RemoveFromParent();
		ActiveModePage = nullptr;
		return false;
	}

	const auto SetText = [](const URokUiPageWidget& Owner, FName Name, const FString& Text)
	{
		if (auto* Label = Owner.Get<UTextBlock>(Name)) Label->SetText(FText::FromString(Text));
	};
	const auto SetIcon = [this](const URokUiPageWidget& Owner, FName Name,
		const FString& TexturePath, const FIntRect& Rect, const FIntPoint& AtlasSize, double Size)
	{
		if (auto* Icon = Owner.Get<UImage>(Name))
		{
			auto* Texture = LoadUiTexture(*TexturePath);
			Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (Texture) Icon->SetBrush(MakeAtlasBrush(Texture, Rect, AtlasSize, FVector2D(Size)));
		}
	};
	SetText(*Page, TEXT("TrainArmyDetailName"), Detail.Name);
	SetText(*Page, TEXT("TrainArmyDetailDescription"), Detail.Description);
	if (!Detail.Attributes.IsEmpty())
	{
		// RS.ArmyTypeIcon keeps cavalry and archer sprite IDs in source order 1,3,2,4.
		const FIntRect TypeRects[] = {SpriteRect(450, 1906, 38, 38), SpriteRect(87, 413, 38, 38),
			SpriteRect(44, 369, 38, 38), SpriteRect(1, 92, 38, 38)};
		SetIcon(*Page, TEXT("TrainArmyDetailTypeIcon"), Detail.Attributes[0].Texture,
			TypeRects[SelectedTrainArmyType - 1], Detail.Attributes[0].AtlasSize, 38);
	}
	for (int32 Index = 0; Index < Detail.Attributes.Num(); ++Index)
	{
		auto* RowSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("TrainArmyDetailAttributeSlot_%d"), Index));
		if (const auto* Row = MountPageItem(RowSlot, AttributeClass, TEXT("TrainArmyDetail"), Index))
		{
			const auto& Attribute = Detail.Attributes[Index];
			SetText(*Row, TEXT("Label"), Attribute.Label);
			SetText(*Row, TEXT("Attribute"), Attribute.Value + (Attribute.bModifierUnknown ? TEXT(" + —") : TEXT("")));
			SetIcon(*Row, TEXT("AttrIcon"), Attribute.Texture, Attribute.Rect, Attribute.AtlasSize, 30);
			BindPageAction(*Row, TEXT("AttrButton"), 1960 + Index);
		}
	}
	for (int32 Index = 0; Index < SkillCount; ++Index)
	{
		auto* IconSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("TrainArmyDetailSkillSlot%d_%d"), SkillCount, Index));
		if (const auto* Item = MountPageItem(IconSlot, SkillClass, TEXT("TrainArmyDetail"), Index))
		{
			const auto& Skill = Detail.Skills[Index];
			SetIcon(*Item, TEXT("SkillIcon"), Skill.Texture, Skill.Rect, Skill.AtlasSize, 72);
			BindPageAction(*Item, TEXT("SkillButton"), 1970 + Index);
		}
	}
	BindPageAction(*Page, TEXT("TrainArmyDetailBackButton"), 1951);
	TrainArmyAnimatedChildren.Add(Panel);
	AdvanceTrainArmyDetailAnimation(0.0f);
	return true;
}

void URokUnityFeatureScreenWidget::AdvanceTrainArmyDetailAnimation(float DeltaSeconds)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::TrainArmy) return;
	TrainArmyDetailAnimationTime = FMath::Min(0.25f, TrainArmyDetailAnimationTime + FMath::Max(0.0f, DeltaSeconds));
	const FVector2D State = RokTrainArmyDetail::Evaluate(TrainArmyDetailAnimationTime, bTrainArmyDetailVisible);
	for (const auto& Child : TrainArmyAnimatedChildren) if (Child.IsValid())
	{
		Child->SetRenderTranslation(FVector2D(State.X, 0));
		Child->SetRenderOpacity(State.Y);
	}
}

void URokUnityFeatureScreenWidget::ShowTrainArmyDetailTip(int32 Index, bool bSkill)
{
	if (!bTrainArmyDetailVisible) return;
	auto* Catalog = GetGameplayCatalogSubsystem();
	if (!Catalog) return;
	const auto Troops = Catalog->GetTroopsForArmyType(SelectedTrainArmyType);
	const auto* Troop = Troops.FindByPredicate([this](const auto& Row) { return Row.Level == SelectedTrainTier; });
	FRokTrainArmyDetail Detail; FString Error;
	if (!Troop || !RokTrainArmyDetail::Load(Troop->Id, Detail, Error)) return;
	if ((bSkill && !Detail.Skills.IsValidIndex(Index)) || (!bSkill && !Detail.Attributes.IsValidIndex(Index))) return;
	if (TrainArmyTipLayer) TrainArmyTipLayer->RemoveFromParent();
	UClass* TipClass = ActiveModePage ? ResolveFeaturePageClass(TEXT("TrainArmyDetailTip")) : nullptr;
	auto* TipPage = TipClass ? CreateWidget<URokUiPageWidget>(this, TipClass) : nullptr;
	UTextBlock* Body = TipPage ? TipPage->Get<UTextBlock>(TEXT("TrainArmyDetailTipText")) : nullptr;
	auto* Content = TipPage ? TipPage->Get<UCanvasPanel>(TEXT("TrainArmyDetailTipContent")) : nullptr;
	auto* Arrow = TipPage ? TipPage->Get<UImage>(TEXT("TrainArmyDetailTipArrow")) : nullptr;
	auto* ContentSlot = Content ? Cast<UCanvasPanelSlot>(Content->Slot) : nullptr;
	auto* ArrowSlot = Arrow ? Cast<UCanvasPanelSlot>(Arrow->Slot) : nullptr;
	const bool bUsePage = Body && ContentSlot && ArrowSlot;
	if (!bUsePage)
	{
		Body = MakeScreenText(TEXT("TrainArmyDetailTipText"), 14, FLinearColor(0.137f, 0.137f, 0.125f, 1));
		Body->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18.0f * 72.0f / 96.0f));
		Body->SetShadowOffset(FVector2D::ZeroVector);
		Body->SetWrapTextAt(500);
		Body->SetAutoWrapText(true);
	}

	const FString Text = bSkill ? Detail.Skills[Index].Tip : Detail.Attributes[Index].Tip;
	Body->SetText(FText::FromString(Text));
	// HelpTip.SetWidth is text width, plus Frame(40,40); offsets reference the
	// clicked button center and include the 12px arrow. Keep source atlas alpha.
	const TSharedRef<SWidget> BodySlate = Body->TakeWidget();
	Body->ForceLayoutPrepass();
	const float Height = Body->GetDesiredSize().Y + 40.0f;
	const float AnchorY = bSkill ? 137 + 511.25f + 42 : 137 + 321 - Detail.Attributes[Index].UnityY;
	const float AnchorX = bSkill ? 618 + RokTrainArmyDetail::SkillCellX(Detail.Skills.Num(), Index) + 50 : 930;
	const float Left = FMath::Clamp(AnchorX - 270.0f, 0.0f, 900.0f);
	const FVector2D Position = CapturePoint(Left, AnchorY - (bSkill ? 30 : 23) - 12 - Height);
	if (bUsePage)
	{
		ContentSlot->SetPosition(Position);
		ContentSlot->SetSize(FVector2D(540, Height));
		ArrowSlot->SetPosition(FVector2D(AnchorX - Left - 7.5f, ArrowSlot->GetPosition().Y));
		BindPageAction(*TipPage, TEXT("TrainArmyDetailTipClickAway"), 1952);
		BindPageAction(*TipPage, TEXT("TrainArmyDetailTipInside"), 1953);
		TipPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		TrainArmyTipLayer = TipPage;
		PlaceOnCanvas(ModeCanvas, TipPage, FVector2D::ZeroVector, CurrentExpandedDesignSize, 100);
		return;
	}
	auto* TipCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TrainArmyDetailTipLayer"));
	TrainArmyTipLayer = TipCanvas;
	PlaceOnCanvas(ModeCanvas, TipCanvas, FVector2D::ZeroVector, CurrentExpandedDesignSize, 100);
	auto* Mask = MakeActionButton(TEXT("TrainArmyDetailTipClickAway"), 1952, nullptr, FIntRect(), FIntPoint(1, 1), CurrentExpandedDesignSize);
	PlaceOnCanvas(TipCanvas, Mask, FVector2D::ZeroVector, CurrentExpandedDesignSize);
	PlaceOnCanvas(TipCanvas, MakeActionButton(TEXT("TrainArmyDetailTipInside"), 1953, nullptr,
		FIntRect(), FIntPoint(1, 1), FVector2D(540, Height)), Position, FVector2D(540, Height), 1);
	PlaceOnCanvas(TipCanvas, MakeAtlasBorder(TEXT("TrainArmyDetailTipBackground"), CommonAtlasPath,
		SpriteRect(774, 276, 72, 72), CommonAtlasSize, FVector2D(540, Height),
		FMargin(21.0f / 72, 23.0f / 72, 21.0f / 72, 24.0f / 72)), Position, FVector2D(540, Height), 2);
	PlaceOnCanvas(TipCanvas, Body, Position + FVector2D(20), FVector2D(500, Height - 40), 3);
	PlaceOnCanvas(TipCanvas, MakeAtlasImage(TEXT("TrainArmyDetailTipArrow"), CommonAtlasPath,
		SpriteRect(120, 137, 15, 12), CommonAtlasSize, FVector2D(15, 12)), Position + FVector2D(AnchorX - Left - 7.5f, Height - 1.4f), FVector2D(15, 12), 3);
}

void URokUnityFeatureScreenWidget::BuildServerBuildingSpeedUp()
{
	UClass* OwnedClass = ResolveFeaturePageClass(TEXT("BuildingSpeedUpOwnedRow"));
	UClass* BuyClass = ResolveFeaturePageClass(TEXT("BuildingSpeedUpBuyRow"));
	auto* Page = OwnedClass && BuyClass ? MountModePage(TEXT("BuildingSpeedUp")) : nullptr;
	if (!Page)
	{
		BuildSplitWindowFrame(CapturePoint(167, 100), FVector2D(1106, 700), NSLOCTEXT("RokSpeedUp", "BuildingSpeed", "建造加速"),
			CapturePoint(1205, 92), nullptr, 1, 0, 30);
	}
	const bool bPreview = GetBuildingSpeedUpPreview() != INDEX_NONE;
	const auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	const auto Queues = ReadBuildingSpeedUpSnapshot(City);
	const auto* Queue = Queues.FindQueue(ServerSpeedUpQueueIndex);
	const bool bQueueMatches = Queue && Queue->BuildingIndex == ServerSpeedUpBuildingIndex;
	if (Page)
	{
		Page->SetShown(Page->Get<UCanvasPanel>(TEXT("BuildingSpeedUpQueueContent")), bQueueMatches);
	}
	if (!bQueueMatches) return;
	int32 BuildingType = bPreview ? 1 : 0, HallLevel = 1;
	if (City)
	{
		for (const auto& Building : City->GetAppliedBuildings())
		{
			if (Building.Index == Queue->BuildingIndex) BuildingType = Building.Type;
			if (Building.Type == 1) HallLevel = Building.Level;
		}
	}
	const FIntPoint Profile = RokBuildingSourcePreview::ResolveProfile(GetGameInstance(), HallLevel);
	RokBuildingSourcePreview::FVisual Art;
	UTexture2D* ArtTexture = RokBuildingSourcePreview::FindVisual(BuildingType, Profile.X, Profile.Y, Art)
		? RokBuildingSourcePreview::LoadTexture(Art) : nullptr;
	auto* Image = Page ? Page->Get<UImage>(TEXT("BuildingSpeedUpIcon")) : nullptr;
	if (ArtTexture)
	{
		const FVector2D Pane(108, 82);
		const FBox2D Draw = Art.FitInPane(Pane * .5f, Pane);
		const FVector2D Position = CapturePoint(222, 161) + Draw.Min;
		if (!Page)
		{
			Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BuildingSpeedUpIcon"));
			PlaceOnCanvas(ModeCanvas, Image, Position, Draw.GetSize(), 4);
		}
		if (Image)
		{
			Image->SetBrushFromTexture(ArtTexture);
			Image->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (auto* ArtSlot = Cast<UCanvasPanelSlot>(Image->Slot))
			{
				ArtSlot->SetPosition(Position);
				ArtSlot->SetSize(Draw.GetSize());
			}
		}
	}
	else if (Image)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
	}

	UScrollBox* List = nullptr;
	if (Page)
	{
		SpeedUpQueueDescriptionText = Page->Get<UTextBlock>(TEXT("SpeedUpQueueDescription"));
		SpeedUpQueueProgress = Page->Get<UProgressBar>(TEXT("SpeedUpQueueProgress"));
		SpeedUpQueueProgressText = Page->Get<UTextBlock>(TEXT("SpeedUpQueueProgressText"));
		List = Page->Get<UScrollBox>(TEXT("BuildingSpeedUpItems"));
	}
	else
	{
		SpeedUpQueueDescriptionText = MakeScreenText(TEXT("SpeedUpQueueDescription"), 24, FLinearColor(.95f, .92f, .76f), true);
		PlaceOnCanvas(ModeCanvas, SpeedUpQueueDescriptionText, CapturePoint(350, 160), FVector2D(476, 35), 5);
		SpeedUpQueueProgress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("SpeedUpQueueProgress"));
		FProgressBarStyle Style;
		auto Track = MakeAtlasBrush(LoadUiTexture(CommonAtlasPath), SpriteRect(401, 112, 20, 23), CommonAtlasSize, FVector2D(20, 23));
		Track.DrawAs = ESlateBrushDrawType::Box;
		Track.Margin = FMargin(.5f, 7.f/23, .5f, 6.f/23);
		Style.SetBackgroundImage(Track).SetFillImage(MakeAtlasBrush(LoadUiTexture(CommonAtlasPath),
			VipProgressFillRect, CommonAtlasSize, FVector2D(12, 19)));
		SpeedUpQueueProgress->SetWidgetStyle(Style);
		PlaceOnCanvas(ModeCanvas, SpeedUpQueueProgress, CapturePoint(350, 210), FVector2D(480, 24), 5);
		SpeedUpQueueProgressText = MakeScreenText(TEXT("SpeedUpQueueProgressText"), 18, FLinearColor::White, true, ETextJustify::Center);
		PlaceOnCanvas(ModeCanvas, SpeedUpQueueProgressText, CapturePoint(350, 209), FVector2D(480, 26), 6);
		List = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BuildingSpeedUpItems"));
		List->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		PlaceOnCanvas(ModeCanvas, List, CapturePoint(205, 263), FVector2D(1030, 503.7f), 4);
	}
	if (!SpeedUpQueueDescriptionText || !SpeedUpQueueProgress || !SpeedUpQueueProgressText || !List) return;
	SpeedUpQueueDescriptionText->SetText(FText::Format(NSLOCTEXT("RokSpeedUp", "BuildingLevelTitle", "等级{0}{1}"),
		FText::AsNumber(Queue->TargetLevel), FText::FromString(Queue->BuildingName)));
	auto* Catalog = GetEconomyCatalogSubsystem();
	TArray<FRokUnityEconomyItem> Items = Catalog ? Catalog->GetItems() : TArray<FRokUnityEconomyItem>();
	Items.RemoveAll([](const FRokUnityEconomyItem& Item)
	{
		return Item.SpeedDurationSeconds <= 0 || (Item.SubType != 20101 && Item.SubType != 20102);
	});
	Items.Sort([](const FRokUnityEconomyItem& A, const FRokUnityEconomyItem& B)
	{
		return A.SubType != B.SubType ? A.SubType > B.SubType : A.Id < B.Id;
	});
	VisibleSpeedUpItemIds.Reset();
	ServerSpeedUpItemCostsGems.Reset();
	FString Failure;
	const bool bCanWrite = bPreview || (Net && Net->CanRequestCityWrite(Failure));
	int64 Gems = bPreview ? 1000 : 0;
	if (!bPreview && Net) RokSprotoJson::TryGetInt64(Net->GetRoleInfo(), TEXT("denar"), Gems);
	const auto* PreviewOwned = bPreview ? Items.FindByPredicate([](const FRokUnityEconomyItem& Item)
	{
		return Item.SubType == 20102 && Item.SpeedDurationSeconds > 12 * 60;
	}) : nullptr;
	const auto ReadCount = [Net, bPreview, PreviewOwned](int32 ItemId, int64& OutCount)
	{
		if (!bPreview) return Net && Net->TryGetOwnedItemCount(ItemId, OutCount);
		OutCount = PreviewOwned && PreviewOwned->Id == ItemId ? 2 : 0;
		return true;
	};
	const auto AddItemRow = [&](const FRokUnityEconomyItem& Item, int64 Count, bool bBuy)
	{
		const int32 Index = VisibleSpeedUpItemIds.Add(Item.Id);
		ServerSpeedUpItemCostsGems.Add(bBuy);
		const bool bEnabled = bCanWrite && !bServerSpeedUpPending && Queue->bCountdownKnown && Queue->RemainingSeconds > 0
			&& (!bBuy || Gems >= Item.ShortcutPrice);
		const TCHAR* IconPath = nullptr;
		FIntRect Rect;
		FIntPoint Size;
		const bool bHasIcon = ResolveEconomyItemIcon(Item, IconPath, Rect, Size);
		const FText ItemName = FText::FromString(Item.DisplayNameZhCn);
		const FText ItemAmount = bBuy ? FText::GetEmpty() : FText::Format(
			NSLOCTEXT("RokSpeedUp", "OwnedCount", "拥有数量:{0}"), FText::AsNumber(Count));
		if (auto* ItemPage = Page ? CreateWidget<URokUiPageWidget>(this, bBuy ? BuyClass : OwnedClass) : nullptr)
		{
			ItemPage->LegacyPrefix = TEXT("SpeedUp");
			ItemPage->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
			if (auto* Icon = ItemPage->Get<UImage>(TEXT("ItemIcon")))
			{
				ItemPage->SetPresent(Icon, bHasIcon);
				if (bHasIcon) Icon->SetBrush(MakeAtlasBrush(LoadUiTexture(IconPath), Rect, Size, FVector2D(82)));
			}
			if (auto* Label = ItemPage->Get<UTextBlock>(TEXT("ItemName"))) Label->SetText(ItemName);
			if (auto* Amount = ItemPage->Get<UTextBlock>(TEXT("ItemCount"))) Amount->SetText(ItemAmount);
			if (auto* Button = ItemPage->Get<URokUnityIndexedButton>(TEXT("ItemAction"))) Button->SetIsEnabled(bEnabled);
			BindPageAction(*ItemPage, TEXT("ItemAction"), 21000 + Index);
			if (bBuy)
			{
				if (auto* Price = ItemPage->Get<UTextBlock>(TEXT("BuyPrice")))
				{
					Price->SetText(FText::AsNumber(Item.ShortcutPrice));
					const bool bAffordable = Gems >= Item.ShortcutPrice;
					FSlateColor PriceColor(bAffordable ? FLinearColor::White : FLinearColor(1, .32f, .26f));
					TryGetFeatureTextColor(bAffordable ? TEXT("BuildingSpeedUpPriceAffordable")
						: TEXT("BuildingSpeedUpPriceInsufficient"), PriceColor);
					Price->SetColorAndOpacity(PriceColor);
				}
			}
			ItemPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			List->AddChild(ItemPage);
			return;
		}
		auto* Row = WidgetTree->ConstructWidget<USizeBox>();
		Row->SetHeightOverride(100);
		auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		Row->AddChild(Canvas);
		List->AddChild(Row);
		if (bHasIcon)
			PlaceOnCanvas(Canvas, MakeAtlasImage(*FString::Printf(TEXT("SpeedUpItemIcon_%d"), Index),
				IconPath, Rect, Size, FVector2D(82)), FVector2D(14, 7), FVector2D(82), 1);
		auto* Name = MakeScreenText(*FString::Printf(TEXT("SpeedUpItemName_%d"), Index), 24, FLinearColor::White, true);
		Name->SetText(ItemName);
		PlaceOnCanvas(Canvas, Name, FVector2D(117, 16), FVector2D(560, 33), 1);
		auto* Amount = MakeScreenText(*FString::Printf(TEXT("SpeedUpItemCount_%d"), Index), 19, FLinearColor(.90f, .87f, .76f));
		Amount->SetText(ItemAmount);
		PlaceOnCanvas(Canvas, Amount, FVector2D(117, 50), FVector2D(460, 28), 1);
		const FVector2D ButtonPosition(804, 14);
		auto* Button = MakeActionButton(*FString::Printf(TEXT("SpeedUpItemAction_%d"), Index), 21000 + Index,
			CommonAtlasPath, bBuy ? MediumGoldButtonRect : MediumBlueButtonRect, CommonAtlasSize, FVector2D(196, 72),
			bBuy ? FText::GetEmpty() : NSLOCTEXT("RokSpeedUp", "UseItem", "使用"), 23,
			FLinearColor(.07f, .13f, .17f), ESlateBrushDrawType::Box, FMargin(.42f, 0, .42f, 0));
		Button->SetIsEnabled(bEnabled);
		PlaceOnCanvas(Canvas, Button, ButtonPosition, FVector2D(196, 72), 2);
		if (bBuy)
		{
			auto* BuyLabel = MakeScreenText(*FString::Printf(TEXT("SpeedUpBuyLabel_%d"), Index), 21,
				FLinearColor(.07f, .13f, .17f), true, ETextJustify::Center);
			BuyLabel->SetText(NSLOCTEXT("RokSpeedUp", "BuyAndUse", "购买并使用"));
			PlaceOnCanvas(Canvas, BuyLabel, ButtonPosition + FVector2D(0, 5), FVector2D(196, 32), 3);
			PlaceOnCanvas(Canvas, MakeAtlasImage(*FString::Printf(TEXT("SpeedUpItemGem_%d"), Index),
				CommonAtlasPath, GemRect, CommonAtlasSize, FVector2D(24)), ButtonPosition + FVector2D(58, 38), FVector2D(24), 3);
			auto* Price = MakeScreenText(*FString::Printf(TEXT("SpeedUpBuyPrice_%d"), Index), 19,
				Gems >= Item.ShortcutPrice ? FLinearColor::White : FLinearColor(1, .32f, .26f), true);
			Price->SetText(FText::AsNumber(Item.ShortcutPrice));
			PlaceOnCanvas(Canvas, Price, ButtonPosition + FVector2D(88, 38), FVector2D(92, 26), 3);
		}
		PlaceOnCanvas(Canvas, MakeAtlasImage(*FString::Printf(TEXT("SpeedUpItemLine_%d"), Index), CommonAtlasPath,
			SpriteRect(1, 611, 570, 2), CommonAtlasSize, FVector2D(995, 2)), FVector2D(12, 97), FVector2D(995, 2), 1);
	};
	for (const FRokUnityEconomyItem& Item : Items)
	{
		int64 Count = 0;
		if (ReadCount(Item.Id, Count) && Count > 0) AddItemRow(Item, Count, false);
	}
	for (const FRokUnityEconomyItem& Item : Items)
	{
		int64 Count = 0;
		if (Item.SubType == 20101 && Item.ShortcutPrice > 0 && ReadCount(Item.Id, Count)
			&& (Count > 0 || Item.ShopPrice > 0)) AddItemRow(Item, 0, true);
	}
	SpeedUpQueueProgress->SetPercent(Queue->Progress);
	SpeedUpQueueProgressText->SetText(Queue->bCountdownKnown ? FormatWorkerCountdown(Queue->RemainingSeconds) : FText::GetEmpty());
}

void URokUnityFeatureScreenWidget::UpdateServerBuildingSpeedUp()
{
	const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	const auto Queues = ReadBuildingSpeedUpSnapshot(City);
	const auto* Queue = Queues.FindQueue(ServerSpeedUpQueueIndex);
	if (!Queue || Queue->BuildingIndex != ServerSpeedUpBuildingIndex || Queue->State != ERokServerBuildQueueState::Busy)
	{
		if (!bServerSpeedUpPending && Queues.bAvailable) RequestCloseScreen();
		return;
	}
	if (SpeedUpQueueProgress) SpeedUpQueueProgress->SetPercent(Queue->Progress);
	if (SpeedUpQueueProgressText) SpeedUpQueueProgressText->SetText(Queue->bCountdownKnown
		? FormatWorkerCountdown(Queue->RemainingSeconds) : FText::GetEmpty());
}

void URokUnityFeatureScreenWidget::HandleServerSpeedUpItem(int32 Index, bool bConfirmed)
{
	if (!VisibleSpeedUpItemIds.IsValidIndex(Index) || !ServerSpeedUpItemCostsGems.IsValidIndex(Index) || bServerSpeedUpPending) return;
	const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	const auto Queues = ReadBuildingSpeedUpSnapshot(City);
	const auto* Queue = Queues.FindQueue(ServerSpeedUpQueueIndex);
	if (!Queue || Queue->BuildingIndex != ServerSpeedUpBuildingIndex || !Queue->bCountdownKnown || Queue->RemainingSeconds <= 0) return;
	const int32 ItemId = VisibleSpeedUpItemIds[Index];
	const bool bBuy = ServerSpeedUpItemCostsGems[Index];
	const auto* Catalog = GetEconomyCatalogSubsystem();
	FRokUnityEconomyItem Item;
	if (!Catalog || !Catalog->FindItemById(ItemId, Item)) return;
	const int32 ExcessMinutes = (Item.SpeedDurationSeconds - FMath::CeilToInt(Queue->RemainingSeconds)) / 60;
	if (!bConfirmed && (bBuy || ExcessMinutes > 10))
	{
		const FText Message = ExcessMinutes > 10
			? FText::Format(NSLOCTEXT("RokSpeedUp", "ConfirmOvertime", "您使用的加速道具已超出所需时间{0}分钟，确定使用吗？"),
				FText::AsNumber(ExcessMinutes))
			: FText::Format(NSLOCTEXT("RokSpeedUp", "ConfirmGemSpend", "消耗{0}宝石使用加速？"), FText::AsNumber(Item.ShortcutPrice));
		UClass* ConfirmClass = ActiveModePage ? ResolveFeaturePageClass(TEXT("BuildingSpeedUpConfirm")) : nullptr;
		if (auto* Dialog = ConfirmClass ? CreateWidget<URokUiPageWidget>(this, ConfirmClass) : nullptr)
		{
			if (auto* Text = Dialog->Get<UTextBlock>(TEXT("BuildingSpeedUpConfirmText")); Text
				&& Dialog->Get<URokUnityIndexedButton>(TEXT("BuildingSpeedUpConfirmCancel"))
				&& Dialog->Get<URokUnityIndexedButton>(TEXT("BuildingSpeedUpConfirmAccept")))
			{
				Text->SetText(Message);
				BindPageAction(*Dialog, TEXT("BuildingSpeedUpConfirmCancel"), 21100);
				BindPageAction(*Dialog, TEXT("BuildingSpeedUpConfirmAccept"), 21101);
				Dialog->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				PlaceOnCanvas(ModeCanvas, Dialog, FVector2D::ZeroVector, CurrentExpandedDesignSize, 90);
				ServerSpeedUpConfirmIndex = Index;
				ServerSpeedUpConfirmation = Dialog;
				return;
			}
		}
		ServerSpeedUpConfirmIndex = Index;
		auto* ConfirmationCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildingSpeedUpConfirmation"));
		ServerSpeedUpConfirmation = ConfirmationCanvas;
		PlaceOnCanvas(ModeCanvas, ConfirmationCanvas, FVector2D::ZeroVector, CurrentExpandedDesignSize, 90);
		auto* Shade = MakeSolidBorder(TEXT("BuildingSpeedUpConfirmShade"), FLinearColor(0, 0, 0, .68f));
		Shade->SetVisibility(ESlateVisibility::Visible);
		PlaceOnCanvas(ConfirmationCanvas, Shade, FVector2D::ZeroVector, CurrentExpandedDesignSize);
		const FVector2D Size(538, 200);
		const FVector2D Origin = (CurrentExpandedDesignSize - Size) * .5f;
		PlaceOnCanvas(ConfirmationCanvas, MakeAtlasBorder(TEXT("BuildingSpeedUpConfirmFrame"), FrameAtlasPath,
			WindowBodyRect, FrameAtlasSize, Size, WindowBodyMargin), Origin, Size, 1);
		auto* Text = MakeScreenText(TEXT("BuildingSpeedUpConfirmText"), 22, FLinearColor::White, false, ETextJustify::Center);
		Text->SetText(Message);
		Text->SetAutoWrapText(true);
		PlaceOnCanvas(ConfirmationCanvas, Text, Origin + FVector2D(20, 42), FVector2D(498, 68), 2);
		PlaceOnCanvas(ConfirmationCanvas, MakeActionButton(TEXT("BuildingSpeedUpConfirmCancel"), 21100,
			CommonAtlasPath, MediumBlueButtonRect, CommonAtlasSize, FVector2D(175, 62), NSLOCTEXT("RokSpeedUp", "Cancel", "取消"), 22),
			Origin + FVector2D(74, 120), FVector2D(175, 62), 3);
		PlaceOnCanvas(ConfirmationCanvas, MakeActionButton(TEXT("BuildingSpeedUpConfirmAccept"), 21101,
			CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize, FVector2D(175, 62), NSLOCTEXT("RokSpeedUp", "Confirm", "确认"), 22),
			Origin + FVector2D(289, 120), FVector2D(175, 62), 3);
		return;
	}
	if (GetBuildingSpeedUpPreview() != INDEX_NONE) return;
	FString Failure;
	auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net) return;
	const TWeakObjectPtr<URokUnityFeatureScreenWidget> WeakThis(this);
	const int32 QueueIndex = ServerSpeedUpQueueIndex;
	const int32 BuildingIndex = ServerSpeedUpBuildingIndex;
	bServerSpeedUpPending = Net->RequestBuildingSpeedUp(QueueIndex, BuildingIndex, ItemId, 1, bBuy,
		[WeakThis, QueueIndex, BuildingIndex](const FString& Result)
		{
			if (auto* Screen = WeakThis.Get(); Screen && Screen->ActiveMode == ERokUnityFeatureScreenMode::SpeedUp
				&& Screen->ServerSpeedUpQueueIndex == QueueIndex && Screen->ServerSpeedUpBuildingIndex == BuildingIndex)
			{
				Screen->bServerSpeedUpPending = false;
				Screen->UpdateServerBuildingSpeedUp();
				if (Screen->ActiveMode != ERokUnityFeatureScreenMode::SpeedUp) return;
				Screen->RebuildModeContent();
				if (!Result.IsEmpty()) Screen->ShowBoundaryFeedback(FText::FromString(Result));
			}
		}, Failure);
	RebuildModeContent();
	if (!Failure.IsEmpty()) ShowBoundaryFeedback(FText::FromString(Failure));
}

void URokUnityFeatureScreenWidget::BuildSpeedUp()
{
	if (ServerSpeedUpQueueIndex > 0)
	{
		BuildServerBuildingSpeedUp();
		return;
	}
	UClass* CardClass = ResolveFeaturePageClass(TEXT("SpeedUpCatalogCard"));
	auto* Page = CardClass ? MountModePage(TEXT("SpeedUp")) : nullptr;
	const FText Title = FText::Format(NSLOCTEXT("RokSpeedUp", "TitleFormat", "{0}加速"),
		ResolveSpeedUpQueueLabel(SpeedUpQueueType));
	if (Page)
	{
		if (auto* Label = Page->Get<UTextBlock>(TEXT("FeatureFrameTitle"))) Label->SetText(Title);
		BindPageAction(*Page, TEXT("SpeedUpPreviousPage"), 1840);
		BindPageAction(*Page, TEXT("SpeedUpNextPage"), 1841);
	}
	else
	{
		BuildSplitWindowFrame(
			CapturePoint(146.0f, 73.0f),
			FVector2D(1148.0f, 754.0f),
			Title,
			CapturePoint(1230.0f, 77.0f));
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				TEXT("SpeedUpContentShade"),
				FLinearColor(0.033f, 0.036f, 0.034f, 0.98f)),
			CapturePoint(162.0f, 145.0f),
			FVector2D(1116.0f, 626.0f),
			2);
	}

	bool bHasActiveQueue = false;
	float QueueProgress = 0.0f;
	int32 QueueRemainingSeconds = 0;
	FText QueueDescription = NSLOCTEXT(
		"RokSpeedUp",
		"CatalogPreviewQueue",
		"未指定活动队列；当前为 Unity 加速道具目录预览。" );
	if (FeatureContextActor.IsValid() && SpeedUpQueueLifecycle.GetStatus() != ERokSpeedUpQueueStatus::Preview)
	{
		if (UWorld* World = GetWorld())
		{
			if (URokBuildingUpgradeSubsystem* UpgradeSubsystem =
					World->GetSubsystem<URokBuildingUpgradeSubsystem>())
			{
				const FRokBuildingState State = UpgradeSubsystem
					->GetBuildingStateForActor(FeatureContextActor.Get());
				const auto BoundStatus = SpeedUpQueueLifecycle.Observe(&State, World->GetTimeSeconds());
				bHasActiveQueue = BoundStatus == ERokSpeedUpQueueStatus::Active
					&& State.bUpgradeInProgress;
				if (bHasActiveQueue)
				{
					QueueProgress = FMath::Clamp(
						UpgradeSubsystem->GetUpgradeProgress(
							FeatureContextActor.Get()),
						0.0f,
						1.0f);
					QueueRemainingSeconds = FMath::Max(
						0,
						FMath::CeilToInt(
							UpgradeSubsystem->GetRemainingUpgradeSeconds(
								FeatureContextActor.Get())));
					QueueDescription = FText::Format(
						NSLOCTEXT(
							"RokSpeedUp",
							"ActiveBuildingQueueFormat",
							"{0}  ·  等级 {1}  ·  剩余 {2}"),
						State.DisplayName,
						FText::AsNumber(State.Level),
						FormatBuildDuration(QueueRemainingSeconds));
				}
				else
				{
					QueueDescription = FText::Format(
						NSLOCTEXT(
							"RokSpeedUp",
							"NoMatchingQueueFormat",
							"{0} 当前没有可确认的 {1} 队列；未伪造倒计时。"),
						State.DisplayName,
						ResolveSpeedUpQueueLabel(SpeedUpQueueType));
				}
			}
		}
	}

	if (Page)
	{
		SpeedUpQueueDescriptionText = Page->Get<UTextBlock>(TEXT("SpeedUpQueueDescription"));
		SpeedUpQueueProgress = Page->Get<UProgressBar>(TEXT("SpeedUpQueueProgress"));
		SpeedUpQueueProgressText = Page->Get<UTextBlock>(TEXT("SpeedUpQueueProgressText"));
	}
	else
	{
		SpeedUpQueueDescriptionText = MakeScreenText(TEXT("SpeedUpQueueDescription"), 20,
			FLinearColor(0.78f, 0.78f, 0.74f, 1.0f), true, ETextJustify::Center);
		SpeedUpQueueDescriptionText->SetAutoWrapText(true);
		PlaceOnCanvas(ModeCanvas, SpeedUpQueueDescriptionText, CapturePoint(195, 158), {1050, 55}, 6);
		SpeedUpQueueProgress = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("SpeedUpQueueProgress"));
		SpeedUpQueueProgress->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(ModeCanvas, SpeedUpQueueProgress, CapturePoint(245, 220), {950, 24}, 6);
		SpeedUpQueueProgressText = MakeScreenText(TEXT("SpeedUpQueueProgressText"), 16,
			FLinearColor::White, true, ETextJustify::Center);
		PlaceOnCanvas(ModeCanvas, SpeedUpQueueProgressText, CapturePoint(245, 217), {950, 28}, 7);
	}
	if (SpeedUpQueueDescriptionText)
	{
		SpeedUpQueueDescriptionText->SetText(QueueDescription);
		SpeedUpQueueDescriptionText->SetColorAndOpacity(bHasActiveQueue
			? FLinearColor(0.94f, 0.90f, 0.65f, 1.0f) : FLinearColor(0.78f, 0.78f, 0.74f, 1.0f));
	}
	if (SpeedUpQueueProgress)
	{
		SpeedUpQueueProgress->SetPercent(QueueProgress);
		SpeedUpQueueProgress->SetFillColorAndOpacity(bHasActiveQueue
			? FLinearColor(0.25f, 0.76f, 0.34f, 1.0f) : FLinearColor(0.25f, 0.28f, 0.26f, 1.0f));
	}
	if (SpeedUpQueueProgressText)
	{
		SpeedUpQueueProgressText->SetText(bHasActiveQueue
			? FText::Format(NSLOCTEXT("RokSpeedUp", "ProgressFormat", "已完成 {0}%"),
				FText::AsNumber(FMath::RoundToInt(QueueProgress * 100.0f)))
			: NSLOCTEXT("RokSpeedUp", "NoProgress", "无活动队列"));
	}

	URokUnityEconomyCatalogSubsystem* EconomyCatalog =
		GetEconomyCatalogSubsystem();
	if (EconomyCatalog && !EconomyCatalog->IsCatalogLoaded())
	{
		EconomyCatalog->LoadCatalog();
	}
	TArray<FRokUnityEconomyItem> SpeedItems = EconomyCatalog
		? EconomyCatalog->GetItems()
		: TArray<FRokUnityEconomyItem>();
	SpeedItems.RemoveAll(
		[this](const FRokUnityEconomyItem& Item)
		{
			return Item.SpeedDurationSeconds <= 0
				|| (Item.SubType != 20101
					&& Item.SubType != SpeedUpQueueType);
		});
	SpeedItems.Sort(
		[](const FRokUnityEconomyItem& Left,
			const FRokUnityEconomyItem& Right)
		{
			if (Left.SpeedDurationSeconds != Right.SpeedDurationSeconds)
			{
				return Left.SpeedDurationSeconds < Right.SpeedDurationSeconds;
			}
			if (Left.SubType != Right.SubType)
			{
				return Left.SubType < Right.SubType;
			}
			return Left.Id < Right.Id;
		});

	if (ReadLocalUiPreviewOption(TEXT("RokSpeedUpCatalogEmpty="), 1, 1) == 1)
	{
		SpeedItems.Reset();
	}

	constexpr int32 ItemsPerPage = 6;
	const int32 PageCount = FMath::Max(
		1,
		FMath::DivideAndRoundUp(SpeedItems.Num(), ItemsPerPage));
	SelectedSpeedUpPage = FMath::Clamp(
		SelectedSpeedUpPage,
		0,
		PageCount - 1);
	VisibleSpeedUpItemIds.Reset();
	const int32 FirstItemIndex = SelectedSpeedUpPage * ItemsPerPage;
	const int32 VisibleItemCount = FMath::Clamp(
		SpeedItems.Num() - FirstItemIndex,
		0,
		ItemsPerPage);
	SelectedSpeedUpItemIndex = VisibleItemCount > 0
		? FMath::Clamp(
			SelectedSpeedUpItemIndex,
			0,
			VisibleItemCount - 1)
		: 0;
	for (int32 LocalIndex = 0; LocalIndex < VisibleItemCount; ++LocalIndex)
	{
		const FRokUnityEconomyItem& Item =
			SpeedItems[FirstItemIndex + LocalIndex];
		VisibleSpeedUpItemIds.Add(Item.Id);
		const int32 Column = LocalIndex % 2;
		const int32 Row = LocalIndex / 2;
		const FVector2D Position = CapturePoint(
			205.0f + Column * 520.0f,
			280.0f + Row * 142.0f);
		const bool bSelected = SelectedSpeedUpItemIndex == LocalIndex;
		const FLinearColor Shade = bSelected
			? FLinearColor(0.24f, 0.20f, 0.10f, 0.98f) : FLinearColor(0.075f, 0.08f, 0.07f, 0.98f);
		const FText ItemName = Item.DisplayNameZhCn.IsEmpty()
			? FText::Format(NSLOCTEXT("RokSpeedUp", "FallbackItemName", "加速道具 {0}"), FText::AsNumber(Item.Id))
			: FText::FromString(Item.DisplayNameZhCn);
		const FText ItemDetails = FText::Format(NSLOCTEXT("RokSpeedUp", "ItemDetailsFormat",
			"加速 {0}  ·  Item {1}\n库存：--（等待服务器背包）"),
			FormatBuildDuration(Item.SpeedDurationSeconds), FText::AsNumber(Item.Id));
		const TCHAR* IconAtlasPath = nullptr;
		FIntRect IconRect;
		FIntPoint IconAtlasSize;
		const bool bHasIcon = ResolveEconomyItemIcon(Item, IconAtlasPath, IconRect, IconAtlasSize);
		if (Page)
		{
			auto* CardSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("SpeedUpCardSlot_%d"), LocalIndex));
			if (const auto* Card = MountPageItem(CardSlot, CardClass, TEXT("SpeedUp"), LocalIndex))
			{
				if (auto* Border = Card->Get<UBorder>(TEXT("ItemShade"))) Border->SetBrushColor(Shade);
				if (auto* Quality = Card->Get<UImage>(TEXT("ItemQuality")))
				{
					Quality->SetBrush(MakeAtlasBrush(LoadUiTexture(Item0AtlasPath),
						ResolveBagQualityRect(Item.Quality), Item0AtlasSize, FVector2D(102)));
				}
				if (auto* Icon = Card->Get<UImage>(TEXT("ItemIcon")))
				{
					Card->SetPresent(Icon, bHasIcon);
					if (bHasIcon) Icon->SetBrush(MakeAtlasBrush(LoadUiTexture(IconAtlasPath), IconRect, IconAtlasSize, FVector2D(92)));
				}
				if (auto* Name = Card->Get<UTextBlock>(TEXT("ItemName"))) Name->SetText(ItemName);
				if (auto* Details = Card->Get<UTextBlock>(TEXT("ItemDetails"))) Details->SetText(ItemDetails);
				BindPageAction(*Card, TEXT("ItemAction"), 1800 + LocalIndex);
				continue;
			}
		}
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				*FString::Printf(TEXT("SpeedUpItemShade_%d"), LocalIndex),
				Shade),
			Position,
			FVector2D(480.0f, 122.0f),
			4);
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				*FString::Printf(TEXT("SpeedUpItemQuality_%d"), LocalIndex),
				Item0AtlasPath,
				ResolveBagQualityRect(Item.Quality),
				Item0AtlasSize,
				FVector2D(102.0f)),
			Position + FVector2D(10.0f, 10.0f),
			FVector2D(102.0f),
			5);
		if (bHasIcon)
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					*FString::Printf(TEXT("SpeedUpItemIcon_%d"), LocalIndex),
					IconAtlasPath,
					IconRect,
					IconAtlasSize,
					FVector2D(92.0f)),
				Position + FVector2D(15.0f, 15.0f),
				FVector2D(92.0f),
				6);
		}
		UTextBlock* Name = MakeScreenText(
			*FString::Printf(TEXT("SpeedUpItemName_%d"), LocalIndex),
			20,
			FLinearColor(0.96f, 0.91f, 0.65f, 1.0f),
			true);
		Name->SetText(ItemName);
		Name->SetAutoWrapText(true);
		PlaceOnCanvas(
			ModeCanvas,
			Name,
			Position + FVector2D(126.0f, 14.0f),
			FVector2D(335.0f, 36.0f),
			7);
		UTextBlock* Details = MakeScreenText(
			*FString::Printf(TEXT("SpeedUpItemDetails_%d"), LocalIndex),
			17,
			FLinearColor(0.80f, 0.80f, 0.77f, 1.0f),
			false);
		Details->SetText(ItemDetails);
		PlaceOnCanvas(
			ModeCanvas,
			Details,
			Position + FVector2D(126.0f, 53.0f),
			FVector2D(335.0f, 58.0f),
			7);
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(TEXT("SpeedUpItemAction_%d"), LocalIndex),
				1800 + LocalIndex,
				nullptr,
				FIntRect(),
				FIntPoint(1, 1),
				FVector2D(480.0f, 122.0f)),
			Position,
			FVector2D(480.0f, 122.0f),
			10);
	}

	const FText PageLabel = FText::Format(NSLOCTEXT("RokSpeedUp", "PageFormat", "{0}/{1}  ·  {2} 项"),
		FText::AsNumber(SelectedSpeedUpPage + 1), FText::AsNumber(PageCount), FText::AsNumber(SpeedItems.Num()));
	if (Page)
	{
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("SpeedUpFallback")), SpeedItems.IsEmpty());
		if (auto* Label = Page->Get<UTextBlock>(TEXT("SpeedUpPage"))) Label->SetText(PageLabel);
		return;
	}

	if (SpeedItems.IsEmpty())
	{
		UTextBlock* Empty = MakeScreenText(
			TEXT("SpeedUpEmptyCatalog"),
			22,
			FLinearColor(0.95f, 0.65f, 0.45f, 1.0f),
			true,
			ETextJustify::Center);
		Empty->SetText(NSLOCTEXT(
			"RokSpeedUp",
			"EmptyCatalog",
			"没有找到适用于此队列的 Unity 加速道具。"));
		PlaceOnCanvas(
			ModeCanvas,
			Empty,
			CapturePoint(300.0f, 450.0f),
			FVector2D(840.0f, 50.0f),
			7);
		URokUnityIndexedButton* EmptyAction = MakeActionButton(
			TEXT("SpeedUpItemAction_0"),
			1800,
			CommonAtlasPath,
			MediumBlueButtonRect,
			CommonAtlasSize,
			FVector2D(260.0f, 68.0f),
			NSLOCTEXT(
				"RokSpeedUp",
				"CatalogUnavailableAction",
				"目录未载入"),
			22);
		EmptyAction->SetIsEnabled(false);
		PlaceOnCanvas(
			ModeCanvas,
			EmptyAction,
			CapturePoint(590.0f, 525.0f),
			FVector2D(260.0f, 68.0f),
			8);
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("SpeedUpPreviousPage"),
			1840,
			CommonAtlasPath,
			MiniBlueButtonRect,
			CommonAtlasSize,
			FVector2D(120.0f, 46.0f),
			NSLOCTEXT("RokSpeedUp", "Previous", "上一页"),
			18),
		CapturePoint(470.0f, 734.0f),
		FVector2D(120.0f, 46.0f),
		8);
	auto* LegacyPageLabel = MakeScreenText(TEXT("SpeedUpPage"), 18, FLinearColor::White, true,
		ETextJustify::Center);
	LegacyPageLabel->SetText(PageLabel);
	PlaceOnCanvas(
		ModeCanvas,
		LegacyPageLabel,
		CapturePoint(595.0f, 741.0f),
		FVector2D(250.0f, 32.0f),
		8);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("SpeedUpNextPage"),
			1841,
			CommonAtlasPath,
			MiniBlueButtonRect,
			CommonAtlasSize,
			FVector2D(120.0f, 46.0f),
			NSLOCTEXT("RokSpeedUp", "Next", "下一页"),
			18),
		CapturePoint(850.0f, 734.0f),
		FVector2D(120.0f, 46.0f),
		8);
}

void URokUnityFeatureScreenWidget::UpdateSpeedUpQueuePresentation()
{
	if (ServerSpeedUpQueueIndex > 0)
	{
		UpdateServerBuildingSpeedUp();
		return;
	}
	if (WindowAnimation.IsClosing()) return;
	if (!SpeedUpQueueDescriptionText
		|| !SpeedUpQueueProgress
		|| !SpeedUpQueueProgressText)
	{
		return;
	}

	bool bHasActiveQueue = false;
	FRokBuildingState ObservedState;
	const FRokBuildingState* ObservedStatePtr = nullptr;
	float QueueProgress = 0.0f;
	int32 QueueRemainingSeconds = 0;
	FText QueueDescription = NSLOCTEXT(
		"RokSpeedUp",
		"CatalogPreviewQueue",
		"未指定活动队列；当前为 Unity 加速道具目录预览。");
	if (FeatureContextActor.IsValid() && SpeedUpQueueLifecycle.GetStatus() != ERokSpeedUpQueueStatus::Preview)
	{
		if (UWorld* World = GetWorld())
		{
			if (URokBuildingUpgradeSubsystem* UpgradeSubsystem =
					World->GetSubsystem<URokBuildingUpgradeSubsystem>())
			{
				ObservedState = UpgradeSubsystem->GetBuildingStateForActor(FeatureContextActor.Get());
				ObservedStatePtr = &ObservedState;
				const FRokBuildingState& State = ObservedState;
				const auto BoundStatus = SpeedUpQueueLifecycle.Observe(&State, World->GetTimeSeconds());
				bHasActiveQueue = BoundStatus == ERokSpeedUpQueueStatus::Active
					&& State.bUpgradeInProgress;
				if (bHasActiveQueue)
				{
					QueueProgress = FMath::Clamp(
						UpgradeSubsystem->GetUpgradeProgress(
							FeatureContextActor.Get()),
						0.0f,
						1.0f);
					QueueRemainingSeconds = FMath::Max(
						0,
						FMath::CeilToInt(
							UpgradeSubsystem->GetRemainingUpgradeSeconds(
								FeatureContextActor.Get())));
					QueueDescription = FText::Format(
						NSLOCTEXT(
							"RokSpeedUp",
							"ActiveBuildingQueueFormat",
							"{0}  ·  等级 {1}  ·  剩余 {2}"),
						State.DisplayName,
						FText::AsNumber(State.Level),
						FormatBuildDuration(QueueRemainingSeconds));
				}
				else
				{
					QueueDescription = FText::Format(
						NSLOCTEXT(
							"RokSpeedUp",
							"NoMatchingQueueFormat",
							"{0} 当前没有可确认的 {1} 队列；未伪造倒计时。"),
						State.DisplayName,
						ResolveSpeedUpQueueLabel(SpeedUpQueueType));
				}
			}
		}
	}

	const auto QueueStatus = SpeedUpQueueLifecycle.Observe(ObservedStatePtr,
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
	if (SpeedUpQueueLifecycle.ConsumeNaturalCompletion())
	{
		SpeedUpQueueProgress->SetPercent(1.0f);
		SpeedUpQueueProgressText->SetText(NSLOCTEXT("RokSpeedUp", "QueueComplete", "已完成 100%"));
		SpeedUpQueueDescriptionText->SetText(NSLOCTEXT("RokSpeedUp", "LocalQueueComplete", "建筑升级完成"));
		UE_LOG(LogTemp, Display, TEXT("Rok speedup bound local queue naturally completed; requesting source window close."));
		RequestCloseScreen();
		return;
	}
	if (QueueStatus == ERokSpeedUpQueueStatus::Lost || QueueStatus == ERokSpeedUpQueueStatus::Replaced)
	{
		bHasActiveQueue = false;
		QueueProgress = 0.0f;
		QueueDescription = QueueStatus == ERokSpeedUpQueueStatus::Lost
			? NSLOCTEXT("RokSpeedUp", "BoundQueueLost", "原建筑队列已不可用；请关闭窗口后重新选择。")
			: NSLOCTEXT("RokSpeedUp", "BoundQueueReplaced", "原建筑队列已变化；请关闭窗口后重新选择。");
	}
	SpeedUpQueueDescriptionText->SetText(QueueDescription);
	SpeedUpQueueDescriptionText->SetColorAndOpacity(FSlateColor(
		bHasActiveQueue
			? FLinearColor(0.94f, 0.90f, 0.65f, 1.0f)
			: FLinearColor(0.78f, 0.78f, 0.74f, 1.0f)));
	SpeedUpQueueProgress->SetPercent(QueueProgress);
	SpeedUpQueueProgress->SetFillColorAndOpacity(
		bHasActiveQueue
			? FLinearColor(0.25f, 0.76f, 0.34f, 1.0f)
			: FLinearColor(0.25f, 0.28f, 0.26f, 1.0f));
	SpeedUpQueueProgressText->SetText(bHasActiveQueue
		? FText::Format(
			NSLOCTEXT("RokSpeedUp", "ProgressFormat", "已完成 {0}%"),
			FText::AsNumber(FMath::RoundToInt(QueueProgress * 100.0f)))
		: NSLOCTEXT("RokSpeedUp", "NoProgress", "无活动队列"));
}

void URokUnityFeatureScreenWidget::UpdateWorkerQueuePresentation()
{
	if (!WorkerQueuePrimaryTitleText || !WorkerQueueSecondaryTitleText) return;
	const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	const auto Queues = ReadWorkerQueuePresentation(City);
	auto* Upgrades = GetWorld() ? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
	AActor* LocalActor = !Queues.bAvailable && Upgrades ? Upgrades->GetFirstActiveUpgradeActor() : nullptr;
	const auto* Primary = Queues.FindQueue(1);
	const auto* Secondary = Queues.FindQueue(2);
	const auto PrimaryState = Queues.bAvailable ? ResolveWorkerQueueState(Primary)
		: (LocalActor ? ERokServerBuildQueueState::Busy : ERokServerBuildQueueState::Idle);
	const auto SecondaryState = ResolveWorkerQueueState(Secondary);
	const auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	FString Failure;
	const bool bCanWrite = Net && Net->CanRequestCityWrite(Failure);
	int64 Items = 0, Gems = 0;
	const bool bItemsKnown = Net && Net->TryGetOwnedItemCount(URokGameNetSubsystem::WorkerHireItemId, Items);
	const bool bUsesItem = bItemsKnown && Items > 0;
	const bool bAffordable = bItemsKnown && (bUsesItem || (RokSprotoJson::TryGetInt64(Net->GetRoleInfo(), TEXT("denar"), Gems)
		&& Gems >= URokGameNetSubsystem::WorkerHireGemCost));
	const bool bRenew = Secondary && SecondaryState == ERokServerBuildQueueState::Idle
		&& Queues.bTimeSynchronized && Secondary->LeaseRemainingSeconds >= 0 && Secondary->LeaseRemainingSeconds < WorkerRequiredSeconds;
	if (Queues.bAvailable != bWorkerQueueRenderedServer || int32(PrimaryState) != WorkerQueueRenderedPrimaryState
		|| int32(SecondaryState) != WorkerQueueRenderedSecondaryState || bCanWrite != bWorkerQueueCanWrite
		|| bUsesItem != bWorkerHireUsesItem || bAffordable != bWorkerHireAffordable
		|| bRenew != bWorkerQueueRenderedRenew || bItemsKnown != bWorkerQueueRenderedItemsKnown)
	{
		RebuildModeContent();
		return;
	}

	const auto UpdateSlot = [](const FRokServerBuildQueueView* Queue, ERokServerBuildQueueState State, bool bSecond,
		bool bExtend, UTextBlock* Title, UTextBlock* Note, UProgressBar* Progress, UTextBlock* Time)
	{
		if (!Title || !Note || !Progress || !Time) return;
		const bool bBusy = State == ERokServerBuildQueueState::Busy;
		Title->SetText(FText::GetEmpty());
		Note->SetText(FText::GetEmpty());
		Time->SetText(FText::GetEmpty());
		Progress->SetVisibility(bBusy ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Progress->SetPercent(bBusy && Queue && Queue->bProgressKnown ? Queue->Progress : 0.f);
		if (bBusy)
		{
			if (Queue && !Queue->BuildingName.IsEmpty())
				Title->SetText(FText::Format(NSLOCTEXT("RokWorkerQueue", "BuildingLevel", "等级{0}{1}正在升级中..."),
					FText::AsNumber(Queue->TargetLevel), FText::FromString(Queue->BuildingName)));
			Note->SetText(NSLOCTEXT("RokWorkerQueue", "Working", "城主，建筑工人们正热火朝天地辛勤工作着。"));
			if (Queue && Queue->bCountdownKnown) Time->SetText(FormatWorkerCountdown(Queue->RemainingSeconds));
		}
		else if (State == ERokServerBuildQueueState::Idle)
		{
			Title->SetText(NSLOCTEXT("RokWorkerQueue", "Idle", "您的建造队列正处于空闲状态"));
			Note->SetText(NSLOCTEXT("RokWorkerQueue", "Ready", "城主，您的建筑工人随时可以开工！"));
			if (bExtend) Note->SetText(NSLOCTEXT("RokWorkerQueue", "InsufficientLease", "建造队列剩余时间不足"));
			else if (bSecond && Queue && Queue->LeaseRemainingSeconds >= 0)
				Note->SetText(FText::Format(NSLOCTEXT("RokWorkerQueue", "LeaseTime", "额外建筑工人剩余时间:{0}"),
					FormatWorkerCountdown(Queue->LeaseRemainingSeconds)));
		}
		else if (State == ERokServerBuildQueueState::Locked)
		{
			Title->SetText(NSLOCTEXT("RokWorkerQueue", "HireAdditional", "雇佣额外的建筑工人，即可同时建造两个建筑！"));
		}
	};
	FRokServerBuildQueueView Local;
	if (LocalActor)
	{
		const FRokBuildingState State = Upgrades->GetBuildingStateForActor(LocalActor);
		Local.BuildingName = State.DisplayName.ToString();
		Local.TargetLevel = State.Level + 1;
		Local.bCountdownKnown = Local.bProgressKnown = true;
		Local.Progress = Upgrades->GetUpgradeProgress(LocalActor);
		Local.RemainingSeconds = Upgrades->GetRemainingUpgradeSeconds(LocalActor);
		Primary = &Local;
	}
	UpdateSlot(Primary, PrimaryState, false, false, WorkerQueuePrimaryTitleText,
		WorkerQueueRuntimeStateText, WorkerQueuePrimaryProgress, WorkerQueuePrimaryProgressText);
	UpdateSlot(Secondary, SecondaryState, true, bRenew, WorkerQueueSecondaryTitleText,
		WorkerQueueSecondaryNoteText, WorkerQueueSecondaryProgress, WorkerQueueSecondaryProgressText);
}

void URokUnityFeatureScreenWidget::HandleWorkerQueueAction(int32 ActionId)
{
	const int32 QueueIndex = ActionId == 1400 ? 1 : 2;
	const auto* City = GetWorld() ? GetWorld()->GetSubsystem<URokServerCitySubsystem>() : nullptr;
	const auto Snapshot = ReadBuildingSpeedUpSnapshot(City);
	if (!Snapshot.bAvailable)
	{
		const auto* Upgrades = GetWorld() ? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
		if (AActor* Actor = Upgrades ? Upgrades->GetFirstActiveUpgradeActor() : nullptr) OpenSpeedUp(Actor, 20102);
		else OpenBuildCity();
		return;
	}
	const auto* Queue = Snapshot.FindQueue(QueueIndex);
	const auto State = ResolveWorkerQueueState(Queue);
	if (State == ERokServerBuildQueueState::Busy && Queue->BuildingIndex > 0)
	{
		ActiveMode = ERokUnityFeatureScreenMode::SpeedUp;
		SpeedUpQueueType = 20102;
		ServerSpeedUpQueueIndex = QueueIndex;
		ServerSpeedUpBuildingIndex = Queue->BuildingIndex;
		bSpeedUpFromWorkerQueue = true;
		bServerSpeedUpPending = false;
		SpeedUpRefreshAccumulator = 0;
		RebuildModeContent(true);
		const int32 Preview = GetBuildingSpeedUpPreview();
		if (Preview == 2 || Preview == 3)
		{
			const int32 Index = ServerSpeedUpItemCostsGems.Find(Preview == 2);
			if (Index != INDEX_NONE)
			{
				const FName Name(*FString::Printf(TEXT("SpeedUpItemAction_%d"), Index));
				if (auto* Button = Cast<UButton>(GetWidgetFromName(Name)); Button && Button->GetIsEnabled())
				{
					Button->OnClicked.Broadcast();
					UE_LOG(LogTemp, Display, TEXT("Rok building speed-up preview: clicked %s."), *Name.ToString());
				}
			}
		}
		return;
	}
	const bool bRenew = QueueIndex == 2 && State == ERokServerBuildQueueState::Idle
		&& Snapshot.bTimeSynchronized && Queue->LeaseRemainingSeconds >= 0 && Queue->LeaseRemainingSeconds < WorkerRequiredSeconds;
	if (QueueIndex == 2 && (State == ERokServerBuildQueueState::Locked || bRenew))
	{
		auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
		FString Failure;
		const TWeakObjectPtr<URokUnityFeatureScreenWidget> WeakThis(this);
		const bool bStarted = Net && Net->RequestWorkerHire(bWorkerHireUsesItem, [WeakThis](const FString& Result)
		{
			if (auto* Screen = WeakThis.Get(); Screen && Screen->ActiveMode == ERokUnityFeatureScreenMode::WorkerQueue)
			{
				Screen->RebuildModeContent();
				if (!Result.IsEmpty()) Screen->ShowBoundaryFeedback(FText::FromString(Result));
			}
		}, Failure);
		UpdateWorkerQueuePresentation();
		if (!bStarted && !Failure.IsEmpty()) ShowBoundaryFeedback(FText::FromString(Failure));
		return;
	}
	if (State == ERokServerBuildQueueState::Idle)
	{
		if (WorkerRequiredSeconds > 0) RequestCloseScreen();
		else OpenBuildCity();
	}
}

void URokUnityFeatureScreenWidget::BuildActivityFrame()
{
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(TEXT("ActivityOuterFrame"), FrameAtlasPath, ActivityFrameRect, FrameAtlasSize, FVector2D(1104.0f, 732.0f), ActivityFrameMargin),
		CapturePoint(168.0f, 84.0f),
		FVector2D(1104.0f, 732.0f),
		0);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("ActivityLeftOpaqueBase"), FLinearColor(0.018f, 0.018f, 0.018f, 0.96f)),
		CapturePoint(182.0f, 101.0f),
		FVector2D(310.0f, 688.0f),
		1);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(TEXT("ActivityLeftPanel"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(310.0f, 688.0f), CommonPanelMargin),
		CapturePoint(182.0f, 101.0f),
		FVector2D(310.0f, 688.0f),
		2);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("ActivityRightUnderlay"), FLinearColor(0.17f, 0.18f, 0.18f, 1.0f)),
		CapturePoint(504.5f, 121.0f),
		FVector2D(735.0f, 658.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("ActivityClose"), 1, CommonAtlasPath, CloseRect, CommonAtlasSize, FVector2D(69.0f)),
		CapturePoint(1222.5f, 56.5f),
		FVector2D(69.0f),
		10);
}

namespace
{
const FText& ActivityMenuTitle(int32 Index)
{
	static const FText Titles[] = {
		NSLOCTEXT("RokFeature", "ActivityCalendar", "活动日历"),
		NSLOCTEXT("RokFeature", "ActivityLordWarA", "城主之战"),
		NSLOCTEXT("RokFeature", "ActivityLordWarB", "城主之战"),
		NSLOCTEXT("RokFeature", "ActivityHarvestMoon", "丰收之月"),
		NSLOCTEXT("RokFeature", "ActivityLogin", "累计登陆")};
	return Titles[Index];
}

const FIntRect& ActivityMenuIconRect(int32 Index)
{
	static const FIntRect IconRects[] = {
		SpriteRect(497, 195, 60, 60),
		SpriteRect(431, 71, 60, 60),
		SpriteRect(431, 71, 60, 60),
		SpriteRect(369, 71, 60, 60),
		SpriteRect(369, 71, 60, 60)};
	return IconRects[Index];
}
}

void URokUnityFeatureScreenWidget::SetActivityIcon(UImage* Image, UTexture2D* Atlas, const FIntRect& Rect) const
{
	if (!Image) return;
	if (!Atlas)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	FSlateBrush Brush = Image->GetBrush();
	const FSlateBrush AtlasBrush = MakeAtlasBrush(Atlas, Rect, ActivityIconAtlasSize, Brush.GetImageSize());
	Brush.SetResourceObject(Atlas);
	Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
	Image->SetBrush(Brush);
}

void URokUnityFeatureScreenWidget::BuildActivityCenter()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok activity screen: tab=%d countdown_seconds=%d authority=local_preview."),
		SelectedActivityIndex,
		FMath::FloorToInt(ActivityPreviewSecondsRemaining));

	UClass* MenuClass = ResolveFeaturePageClass(TEXT("ActivityMenuRow"));
	UClass* CalendarRowClass = ResolveFeaturePageClass(TEXT("ActivityCalendarRow"));
	if (const URokUiPageWidget* Page = MenuClass && CalendarRowClass ? MountModePage(NAME_None, false) : nullptr)
	{
		UClass* PresentationClass = ResolveFeaturePageClass(TEXT("ActivityListPresentation"));
		const auto* Presentation = PresentationClass ? CreateWidget<URokUiPageWidget>(this, PresentationClass) : nullptr;
		BindPageAction(*Page, TEXT("ActivityClose"), 1);
		const bool bCalendar = SelectedActivityIndex == 0;
		const FName LordsWarContent[] = {
			TEXT("ActivityLordsWarBackground"), TEXT("ActivityTitle"), TEXT("ActivityTagline"),
			TEXT("ActivityDescriptionShade"), TEXT("ActivityDescription"), TEXT("ActivityRewardGem"),
			TEXT("ActivityRewardHero"), TEXT("ActivityRewardSpeed"), TEXT("ActivityRewardChest"),
			TEXT("ActivityCountdown"), TEXT("ActivityHistoryButton")
		};
		for (FName Name : LordsWarContent)
		{
			Page->SetPresent(Page->Get<UWidget>(Name), !bCalendar);
		}
		if (bCalendar)
		{
			BuildActivityCalendarReadonly(Page, CalendarRowClass, Presentation);
		}
		else
		{
			for (const FName Name : {FName(TEXT("ActivityCalendarReferencePanel")), FName(TEXT("ActivityCalendarReferenceScope")),
				FName(TEXT("ActivityCalendarReferenceScroll")), FName(TEXT("ActivityCalendarSelectedIcon")),
				FName(TEXT("ActivityCalendarSelectedNeutralIcon")), FName(TEXT("ActivityCalendarSelectedTitle")),
				FName(TEXT("ActivityCalendarSelectedSketch")), FName(TEXT("ActivityCalendarSourceUnavailable"))})
			{
				Page->SetPresent(Page->Get<UWidget>(Name), false);
			}
			ActivityCountdownText = Page->Get<UTextBlock>(TEXT("ActivityCountdown"));
			LastRenderedActivitySecond = INDEX_NONE;
			UpdateActivityCountdown(0.0f);
			BindPageAction(*Page, TEXT("ActivityHistoryButton"), 190);
		}

		UTexture2D* Atlas = LoadUiTexture(ActivityIconAtlasPath);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			UCanvasPanel* MenuSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("ActivityMenuSlot_%d"), Index));
			if (const URokUiPageWidget* Row = MountPageItem(MenuSlot, MenuClass, TEXT("ActivityMenu"), Index))
			{
				const bool bSelected = Index == SelectedActivityIndex;
				Row->SetPresent(Row->Get<UCanvasPanel>(TEXT("Background")), !bSelected);
				Row->SetPresent(Row->Get<UCanvasPanel>(TEXT("SelectedBackground")), bSelected);
				SetActivityIcon(Row->Get<UImage>(TEXT("Icon")), Atlas, ActivityMenuIconRect(Index));
				if (UTextBlock* Label = Row->Get<UTextBlock>(TEXT("Label")))
				{
					Label->SetText(ActivityMenuTitle(Index));
					Label->SetColorAndOpacity(FeaturePresentationColor(Presentation,
						bSelected ? TEXT("ActivityMenuSelected") : TEXT("ActivityMenuNormal"),
						bSelected ? FLinearColor(0.40f, 0.39f, 0.33f, 1.0f) : FLinearColor::White));
				}
				Row->SetPresent(Row->Get<UImage>(TEXT("New")), Index != 1);
				Row->SetPresent(Row->Get<UTextBlock>(TEXT("NewText")), Index != 1);
				Row->SetPresent(Row->Get<UImage>(TEXT("Red")), Index >= 3);
				UTextBlock* Count = Row->Get<UTextBlock>(TEXT("RedCount"));
				Row->SetPresent(Count, Index >= 3);
				if (Count)
				{
					Count->SetText(FText::AsNumber(Index == 3 ? 8 : 1));
				}
				BindPageAction(*Row, TEXT("Hit"), 100 + Index);
			}
		}
		return;
	}

	BuildActivityFrame();
	if (SelectedActivityIndex == 0)
	{
		BuildActivityCalendarReadonly();
	}
	else
	{

	UImage* Background = MakeAtlasImage(TEXT("ActivityLordsWarBackground"), ActivityImageAtlasPath, SpriteRect(1, 1403, 716, 644), ActivityImageAtlasSize, FVector2D(714.0f, 644.0f));
	PlaceOnCanvas(ModeCanvas, Background, CapturePoint(515.0f, 128.0f), FVector2D(714.0f, 644.0f), 2);

	UTextBlock* Title = MakeScreenText(TEXT("ActivityTitle"), 40, FLinearColor::White, true, ETextJustify::Center);
	Title->SetText(NSLOCTEXT("RokFeature", "ActivityLordsWar", "城主之战"));
	PlaceOnCanvas(ModeCanvas, Title, CapturePoint(522.0f, 159.0f), FVector2D(700.0f, 50.0f), 5);
	UTextBlock* Tagline = MakeScreenText(TEXT("ActivityTagline"), 27, FLinearColor::White, true, ETextJustify::Center);
	Tagline->SetText(NSLOCTEXT("RokFeature", "ActivityComingSoon", "即将到来"));
	PlaceOnCanvas(ModeCanvas, Tagline, CapturePoint(522.0f, 214.0f), FVector2D(700.0f, 34.0f), 5);

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("ActivityDescriptionShade"), FLinearColor(0.08f, 0.06f, 0.06f, 0.68f)),
		CapturePoint(529.0f, 400.0f),
		FVector2D(663.0f, 244.0f),
		3);
	UTextBlock* Description = MakeScreenText(TEXT("ActivityDescription"), 16, FLinearColor(0.93f, 0.92f, 0.9f, 1.0f), false, ETextJustify::Center);
	Description->SetText(NSLOCTEXT(
		"RokFeature",
		"ActivityLordsWarRule",
		"为了寻找文明历史上最为强大的领袖，一场特别活动即将面向王国内的所有城主开放。整个活动将会分为5个阶段，所有参与并完成阶段目标的城主都能获得道具奖励。而最终进入排行榜前列的城主们，将能够赢得令人钦羡的丰厚战利品。"));
	Description->SetAutoWrapText(true);
	Description->SetWrapTextAt(780.0f);
	Description->SetLineHeightPercentage(0.95f);
	Description->SetRenderScale(FVector2D(0.85f, 1.0f));
	PlaceOnCanvas(ModeCanvas, Description, CapturePoint(541.0f, 431.9f), FVector2D(780.0f, 91.0f), 5);

	UCanvasPanel* RewardA = MakeRewardCell(TEXT("ActivityRewardGem"), Item1AtlasPath, SpriteRect(1, 513, 126, 126), Item1AtlasSize, FText::AsNumber(1), FText::FromString(TEXT("2,000")), 1, FVector2D(85.0f));
	UCanvasPanel* RewardB = MakeRewardCell(TEXT("ActivityRewardHero"), Item5AtlasPath, SpriteRect(129, 1281, 126, 126), Item5AtlasSize, FText::AsNumber(180), FText::GetEmpty(), 2, FVector2D(85.0f));
	UCanvasPanel* RewardC = MakeRewardCell(TEXT("ActivityRewardSpeed"), Item2AtlasPath, SpriteRect(1, 1, 126, 126), Item2AtlasSize, FText::AsNumber(35), NSLOCTEXT("RokFeature", "SixtyMinutes", "60分钟"), 0, FVector2D(85.0f));
	UCanvasPanel* RewardD = MakeRewardCell(TEXT("ActivityRewardChest"), Item1AtlasPath, SpriteRect(1, 897, 126, 126), Item1AtlasSize, FText::AsNumber(100), FText::GetEmpty(), 0, FVector2D(85.0f));
	PlaceOnCanvas(ModeCanvas, RewardA, CapturePoint(692.0f, 545.0f), FVector2D(85.0f), 6);
	PlaceOnCanvas(ModeCanvas, RewardB, CapturePoint(783.0f, 545.0f), FVector2D(85.0f), 6);
	PlaceOnCanvas(ModeCanvas, RewardC, CapturePoint(874.0f, 545.0f), FVector2D(85.0f), 6);
	PlaceOnCanvas(ModeCanvas, RewardD, CapturePoint(965.0f, 545.0f), FVector2D(85.0f), 6);

	ActivityCountdownText = MakeScreenText(TEXT("ActivityCountdown"), 18, FLinearColor::White, false, ETextJustify::Center);
	PlaceOnCanvas(ModeCanvas, ActivityCountdownText, CapturePoint(522.0f, 647.7f), FVector2D(700.0f, 30.0f), 6);
	LastRenderedActivitySecond = INDEX_NONE;
	UpdateActivityCountdown(0.0f);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("ActivityHistoryButton"),
			190,
			CommonAtlasPath,
			LargeBlueButtonRect,
			CommonAtlasSize,
			FVector2D(274.0f, 84.0f),
			NSLOCTEXT("RokFeature", "ViewHistoryRank", "查看历史排名"),
			27,
			FLinearColor(0.03f, 0.16f, 0.26f, 1.0f)),
		CapturePoint(735.0f, 672.0f),
		FVector2D(274.0f, 84.0f),
		7);
	}

	for (int32 Index = 0; Index < 5; ++Index)
	{
		UCanvasPanel* Row = MakeActivityMenuRow(Index);
		static const float YPositions[] = {130.0f, 370.0f, 480.0f, 590.0f, 700.0f};
		PlaceOnCanvas(ModeCanvas, Row, CapturePoint(194.0f, YPositions[Index]), FVector2D(288.0f, 100.0f), 6);
	}
	UTextBlock* Popular = MakeScreenText(TEXT("ActivityPopularHeader"), 22, FLinearColor(0.55f, 0.55f, 0.55f, 1.0f), false, ETextJustify::Center);
	Popular->SetText(NSLOCTEXT("RokFeature", "PopularActivities", "热门活动"));
	PlaceOnCanvas(ModeCanvas, Popular, CapturePoint(194.0f, 316.0f), FVector2D(288.0f, 38.0f), 7);
	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("ActivityPopularLine"), CommonAtlasPath, GroupLineRect, CommonAtlasSize, FVector2D(288.0f, 2.0f)), CapturePoint(194.0f, 351.0f), FVector2D(288.0f, 2.0f), 7);

	// The activity schedule, red-point counts and countdown are server event
	// data; the whole card is a local preview and must say so.
	UTextBlock* ActivityPreviewTag = MakeScreenText(TEXT("ActivityPreviewTag"), 14, FLinearColor(0.62f, 0.62f, 0.58f, 1.0f), false, ETextJustify::Right);
	ActivityPreviewTag->SetText(NSLOCTEXT("RokFeature", "ActivityPreviewTag", "活动 · 本地参考态（非账号状态）"));
	PlaceOnCanvas(ModeCanvas, ActivityPreviewTag, CapturePoint(900.0f, 700.0f), FVector2D(328.0f, 24.0f), 7);

}

void URokUnityFeatureScreenWidget::BuildActivityCalendarReadonly(const URokUiPageWidget* Page, UClass* RowClass,
	const URokUiPageWidget* Presentation)
{
	URokUnityGameplayCatalogSubsystem* Catalog = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
	const bool bLoaded = Catalog && Catalog->LoadActivityCalendarReadonly();
	if (Page)
	{
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("ActivityCalendarSourceUnavailable")), !bLoaded);
		if (!bLoaded)
		{
			for (const FName Name : {FName(TEXT("ActivityCalendarReferencePanel")), FName(TEXT("ActivityCalendarReferenceScope")),
				FName(TEXT("ActivityCalendarReferenceScroll")), FName(TEXT("ActivityCalendarSelectedIcon")),
				FName(TEXT("ActivityCalendarSelectedNeutralIcon")), FName(TEXT("ActivityCalendarSelectedTitle")),
				FName(TEXT("ActivityCalendarSelectedSketch"))})
			{
				Page->SetPresent(Page->Get<UWidget>(Name), false);
			}
			return;
		}
	}
	else if (!bLoaded)
	{
		UTextBlock* Error = MakeScreenText(TEXT("ActivityCalendarSourceUnavailable"), 22,
			FLinearColor::White, false, ETextJustify::Center);
		Error->SetText(NSLOCTEXT("RokFeature", "ActivityCalendarSourceUnavailable",
			"活动配置暂不可用"));
		PlaceOnCanvas(ModeCanvas, Error, CapturePoint(555.0f, 320.0f), FVector2D(630.0f, 60.0f), 5);
		return;
	}
	const TArray<FRokUnityActivityCalendarReadonlyRow>& Rows = Catalog->GetActivityCalendarReadonlyRows();
	if (Rows.Num() != 89) return;
	if (SelectedActivityCalendarId == 0) SelectedActivityCalendarId = Rows[0].Id;
	const FRokUnityActivityCalendarReadonlyRow* Selected = nullptr;
	for (const FRokUnityActivityCalendarReadonlyRow& Row : Rows)
	{
		if (Row.Id == SelectedActivityCalendarId) { Selected = &Row; break; }
	}
	if (!Selected) return;
	if (Page)
	{
		UScrollBox* Scroll = Page->Get<UScrollBox>(TEXT("ActivityCalendarReferenceScroll"));
		USizeBox* Extent = Page->Get<USizeBox>(TEXT("ActivityCalendarReferenceExtent"));
		UCanvasPanel* Content = Page->Get<UCanvasPanel>(TEXT("ActivityCalendarReferenceContent"));
		if (!Scroll || !Extent || !Content) return;
		FVector2D RowSize(324.0, 110.0);
		double RowGap = 0.0;
		if (Presentation)
		{
			if (const auto* Bounds = Presentation->Get<UWidget>(TEXT("CalendarRowBounds")))
			{
				if (const auto* Slot = Cast<UCanvasPanelSlot>(Bounds->Slot); Slot && Slot->GetSize().X > 0 && Slot->GetSize().Y > 0)
				{
					RowSize = Slot->GetSize();
				}
			}
			if (const auto* Gap = Presentation->Get<UWidget>(TEXT("CalendarRowGap")))
			{
				if (const auto* Slot = Cast<UCanvasPanelSlot>(Gap->Slot))
				{
					RowGap = FMath::Max(0.0, Slot->GetSize().Y);
				}
			}
		}
		const double RowStride = RowSize.Y + RowGap;
		Extent->SetHeightOverride(static_cast<float>(RowSize.Y + (Rows.Num() - 1) * RowStride));
		UTexture2D* Atlas = LoadUiTexture(ActivityIconAtlasPath);
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const FRokUnityActivityCalendarReadonlyRow& Row = Rows[Index];
			URokUiPageWidget* Item = CreateWidget<URokUiPageWidget>(this, RowClass);
			if (!Item) continue;
			Item->LegacyPrefix = TEXT("ActivityCalendar");
			Item->LegacySuffix = FString::Printf(TEXT("_%d"), Row.Id);
			Item->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			PlaceOnCanvas(Content, Item, FVector2D(0.0, Index * RowStride), RowSize, 1);
			const bool bSelected = Row.Id == SelectedActivityCalendarId;
			Item->SetPresent(Item->Get<UCanvasPanel>(TEXT("ReferenceRowFrame")), !bSelected);
			Item->SetPresent(Item->Get<UCanvasPanel>(TEXT("SelectedRowFrame")), bSelected);
			FIntRect IconRect;
			const bool bHasIcon = Atlas && ResolveActivityCalendarIcon(Row.IconReference, IconRect);
			UImage* Icon = Item->Get<UImage>(TEXT("ReferenceIcon"));
			Item->SetPresent(Icon, bHasIcon);
			Item->SetPresent(Item->Get<UCanvasPanel>(TEXT("NeutralIcon")), !bHasIcon);
			if (bHasIcon)
			{
				SetActivityIcon(Icon, Atlas, IconRect);
			}
			if (UTextBlock* Title = Item->Get<UTextBlock>(TEXT("ReferenceTitle")))
			{
				Title->SetText(FText::FromString(Row.TitleZhCn));
				Title->SetColorAndOpacity(FeaturePresentationColor(Presentation,
					bSelected ? TEXT("CalendarTitleSelected") : TEXT("CalendarTitleNormal"),
					bSelected ? FLinearColor(0.33f, 0.30f, 0.25f, 1.0f) : FLinearColor::White));
			}
			BindPageAction(*Item, TEXT("ReferenceHit"), 16000 + Index);
		}
		Scroll->SetScrollOffset(ActivityCalendarScrollOffset);
		FIntRect DetailRect;
		const bool bHasDetailIcon = Atlas && ResolveActivityCalendarIcon(Selected->IconReference, DetailRect);
		UImage* DetailIcon = Page->Get<UImage>(TEXT("ActivityCalendarSelectedIcon"));
		Page->SetPresent(DetailIcon, bHasDetailIcon);
		Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("ActivityCalendarSelectedNeutralIcon")), !bHasDetailIcon);
		if (bHasDetailIcon)
		{
			SetActivityIcon(DetailIcon, Atlas, DetailRect);
		}
		SetFeaturePageText(*Page, TEXT("ActivityCalendarSelectedTitle"), FText::FromString(Selected->TitleZhCn));
		UTextBlock* Sketch = Page->Get<UTextBlock>(TEXT("ActivityCalendarSelectedSketch"));
		Page->SetPresent(Sketch, !Selected->SketchZhCn.IsEmpty());
		if (Sketch)
		{
			Sketch->SetText(FText::FromString(Selected->SketchZhCn));
		}
		return;
	}

	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("ActivityCalendarReferencePanel"),
		CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(700.0f, 550.0f),
		CommonPanelMargin), CapturePoint(523.0f, 128.0f), FVector2D(700.0f, 550.0f), 2);
	UTextBlock* Scope = MakeScreenText(TEXT("ActivityCalendarReferenceScope"), 16,
		FLinearColor(0.32f, 0.29f, 0.24f, 1.0f), false, ETextJustify::Center);
	Scope->SetText(NSLOCTEXT("RokFeature", "ActivityCalendarStaticOnly",
		"活动配置资料预览 · 活动时间待服务器发布"));
	PlaceOnCanvas(ModeCanvas, Scope, CapturePoint(546.0f, 145.0f), FVector2D(650.0f, 34.0f), 4);

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
		TEXT("ActivityCalendarReferenceScroll"));
	Scroll->SetOrientation(Orient_Vertical);
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	Scroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(ModeCanvas, Scroll, CapturePoint(541.0f, 190.0f), FVector2D(324.0f, 452.0f), 5);
	USizeBox* Extent = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
		TEXT("ActivityCalendarReferenceExtent"));
	Extent->SetWidthOverride(324.0f);
	Extent->SetHeightOverride(Rows.Num() * 110.0f);
	Scroll->AddChild(Extent);
	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
		TEXT("ActivityCalendarReferenceContent"));
	Content->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Extent->AddChild(Content);
	const bool bAtlasAvailable = LoadObject<UTexture2D>(nullptr, ActivityIconAtlasPath) != nullptr;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FRokUnityActivityCalendarReadonlyRow& Row = Rows[Index];
		UCanvasPanel* Item = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("ActivityCalendarReferenceRow_%d"), Row.Id));
		PlaceOnCanvas(Content, Item, FVector2D(0.0f, Index * 110.0f), FVector2D(324.0f, 110.0f), 1);
		PlaceOnCanvas(Item, MakeAtlasBorder(*FString::Printf(TEXT("ActivityCalendarReferenceRowFrame_%d"), Row.Id),
			CommonAtlasPath, Row.Id == SelectedActivityCalendarId ? SideSelectedRect : SideNormalRect,
			CommonAtlasSize, FVector2D(324.0f, 110.0f),
			Row.Id == SelectedActivityCalendarId ? SideSelectedMargin : SideNormalMargin),
			FVector2D::ZeroVector, FVector2D(324.0f, 110.0f), 1);
		FIntRect IconRect;
		if (bAtlasAvailable && ResolveActivityCalendarIcon(Row.IconReference, IconRect))
		{
			PlaceOnCanvas(Item, MakeAtlasImage(*FString::Printf(TEXT("ActivityCalendarReferenceIcon_%d"), Row.Id),
				ActivityIconAtlasPath, IconRect, ActivityIconAtlasSize, FVector2D(60.0f)),
				FVector2D(10.0f, 25.0f), FVector2D(60.0f), 2);
		}
		else
		{
			PlaceOnCanvas(Item, MakeAtlasBorder(*FString::Printf(TEXT("ActivityCalendarNeutralIcon_%d"), Row.Id),
				CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(60.0f), CommonPanelMargin),
				FVector2D(10.0f, 25.0f), FVector2D(60.0f), 2);
		}
		UTextBlock* Label = MakeScreenText(*FString::Printf(TEXT("ActivityCalendarReferenceTitle_%d"), Row.Id),
			20, Row.Id == SelectedActivityCalendarId
				? FLinearColor(0.33f, 0.30f, 0.25f, 1.0f) : FLinearColor::White, true);
		Label->SetText(FText::FromString(Row.TitleZhCn));
		PlaceOnCanvas(Item, Label, FVector2D(78.0f, 35.0f), FVector2D(230.0f, 40.0f), 3);
		PlaceOnCanvas(Item, MakeActionButton(*FString::Printf(TEXT("ActivityCalendarReferenceHit_%d"), Row.Id),
			16000 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(324.0f, 110.0f)),
			FVector2D::ZeroVector, FVector2D(324.0f, 110.0f), 4);
	}
	Scroll->SetScrollOffset(ActivityCalendarScrollOffset);

	FIntRect SelectedIconRect;
	if (bAtlasAvailable && ResolveActivityCalendarIcon(Selected->IconReference, SelectedIconRect))
	{
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("ActivityCalendarSelectedIcon"), ActivityIconAtlasPath,
			SelectedIconRect, ActivityIconAtlasSize, FVector2D(76.0f)),
			CapturePoint(900.0f, 207.0f), FVector2D(76.0f), 5);
	}
	else
	{
		PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("ActivityCalendarSelectedNeutralIcon"),
			CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(76.0f), CommonPanelMargin),
			CapturePoint(900.0f, 207.0f), FVector2D(76.0f), 5);
	}
	UTextBlock* Title = MakeScreenText(TEXT("ActivityCalendarSelectedTitle"), 26,
		FLinearColor(0.30f, 0.27f, 0.23f, 1.0f), true, ETextJustify::Center);
	Title->SetText(FText::FromString(Selected->TitleZhCn));
	PlaceOnCanvas(ModeCanvas, Title, CapturePoint(880.0f, 300.0f), FVector2D(315.0f, 60.0f), 5);
	if (!Selected->SketchZhCn.IsEmpty())
	{
		UTextBlock* Sketch = MakeScreenText(TEXT("ActivityCalendarSelectedSketch"), 17,
			FLinearColor(0.32f, 0.29f, 0.25f, 1.0f), false, ETextJustify::Center);
		Sketch->SetText(FText::FromString(Selected->SketchZhCn));
		Sketch->SetAutoWrapText(true);
		Sketch->SetWrapTextAt(300.0f);
		PlaceOnCanvas(ModeCanvas, Sketch, CapturePoint(880.0f, 385.0f), FVector2D(315.0f, 180.0f), 5);
	}
}

UCanvasPanel* URokUnityFeatureScreenWidget::MakeActivityMenuRow(int32 Index)
{
	const bool bSelected = Index == SelectedActivityIndex;
	UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("ActivityMenuRow_%d"), Index));
	Row->SetClipping(EWidgetClipping::Inherit);
	const FVector2D BackgroundSize = bSelected ? FVector2D(313.58f, 104.0f) : FVector2D(288.0f, 100.0f);
	UCanvasPanel* RowBackground = MakeAtlasBorder(
		*FString::Printf(TEXT("ActivityMenuBackground_%d"), Index),
		CommonAtlasPath,
		bSelected ? SideSelectedRect : SideNormalRect,
		CommonAtlasSize,
		BackgroundSize,
		bSelected ? SideSelectedMargin : SideNormalMargin);
	if (bSelected)
	{
		RowBackground->SetRenderTransformPivot(FVector2D(0.5f));
		RowBackground->SetRenderScale(FVector2D(-1.0f, 1.0f));
	}
	PlaceOnCanvas(
		Row,
		RowBackground,
		FVector2D::ZeroVector,
		BackgroundSize);
	PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("ActivityMenuIcon_%d"), Index), ActivityIconAtlasPath, ActivityMenuIconRect(Index), ActivityIconAtlasSize, FVector2D(60.0f)), FVector2D(14.0f, 20.0f), FVector2D(60.0f), 2);
	UTextBlock* Label = MakeScreenText(*FString::Printf(TEXT("ActivityMenuLabel_%d"), Index), 20, bSelected ? FLinearColor(0.40f, 0.39f, 0.33f, 1.0f) : FLinearColor::White, true);
	Label->SetText(ActivityMenuTitle(Index));
	PlaceOnCanvas(Row, Label, FVector2D(86.0f, 32.0f), FVector2D(178.0f, 38.0f), 3);
	if (Index == 0 || Index == 2 || Index >= 3)
	{
		PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("ActivityMenuNew_%d"), Index), CommonAtlasPath, NewRect, CommonAtlasSize, FVector2D(53.0f, 56.0f)), FVector2D(-6.0f, -8.0f), FVector2D(53.0f, 56.0f), 4);
		UTextBlock* NewText = MakeScreenText(*FString::Printf(TEXT("ActivityMenuNewText_%d"), Index), 19, FLinearColor::White, true, ETextJustify::Center);
		NewText->SetText(NSLOCTEXT("RokFeature", "NewBadge", "新"));
		PlaceOnCanvas(Row, NewText, FVector2D(2.0f, 3.0f), FVector2D(38.0f, 30.0f), 5);
	}
	if (Index == 3 || Index == 4)
	{
		PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("ActivityMenuRed_%d"), Index), CommonAtlasPath, RedPointRect, CommonAtlasSize, FVector2D(27.0f, 29.0f)), FVector2D(258.0f, 2.0f), FVector2D(27.0f, 29.0f), 4);
		UTextBlock* Count = MakeScreenText(*FString::Printf(TEXT("ActivityMenuRedCount_%d"), Index), 15, FLinearColor::White, true, ETextJustify::Center);
		Count->SetText(FText::AsNumber(Index == 3 ? 8 : 1));
		PlaceOnCanvas(Row, Count, FVector2D(258.0f, 3.0f), FVector2D(27.0f, 22.0f), 5);
	}
	PlaceOnCanvas(Row, MakeActionButton(*FString::Printf(TEXT("ActivityMenuHit_%d"), Index), 100 + Index, nullptr, FIntRect(), FIntPoint(1, 1), FVector2D(288.0f, 100.0f)), FVector2D::ZeroVector, FVector2D(288.0f, 100.0f), 10);
	return Row;
}

void URokUnityFeatureScreenWidget::BuildSplitWindowFrame(
	const FVector2D& Position,
	const FVector2D& Size,
	const FText& Title,
	const FVector2D& ClosePosition, UCanvasPanel* Parent, int32 CloseAction, float BodyInset, int32 SourceTitlePixels)
{
	if (!Parent) Parent = ModeCanvas;
	const float HalfWidth = Size.X * 0.5f;
	const FVector2D BodyPosition(Position.X + BodyInset, Position.Y + 52.0f);
	const FVector2D BodySize(HalfWidth - BodyInset, Size.Y - 62.0f);
	UCanvasPanel* BodyLeft = MakeAtlasBorder(TEXT("FeatureFrameBodyLeft"), FrameAtlasPath, WindowBodyRect, FrameAtlasSize, BodySize, WindowBodyMargin);
	PlaceOnCanvas(Parent, BodyLeft, BodyPosition, BodySize, 1);
	UCanvasPanel* BodyRight = MakeAtlasBorder(TEXT("FeatureFrameBodyRight"), FrameAtlasPath, WindowBodyRect, FrameAtlasSize, BodySize, WindowBodyMargin);
	BodyRight->SetRenderTransformPivot(FVector2D(0.5f));
	BodyRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(Parent, BodyRight, BodyPosition + FVector2D(BodySize.X, 0.0f), BodySize, 1);

	UCanvasPanel* HeaderLeft = MakeAtlasBorder(TEXT("FeatureFrameHeaderLeft"), FrameAtlasPath, WindowHeaderRect, FrameAtlasSize, FVector2D(HalfWidth, 60.0f), WindowHeaderMargin);
	PlaceOnCanvas(Parent, HeaderLeft, Position, FVector2D(HalfWidth, 60.0f), 5);
	UCanvasPanel* HeaderRight = MakeAtlasBorder(TEXT("FeatureFrameHeaderRight"), FrameAtlasPath, WindowHeaderRect, FrameAtlasSize, FVector2D(HalfWidth, 60.0f), WindowHeaderMargin);
	HeaderRight->SetRenderTransformPivot(FVector2D(0.5f));
	HeaderRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(Parent, HeaderRight, FVector2D(Position.X + HalfWidth, Position.Y), FVector2D(HalfWidth, 60.0f), 5);

	UTextBlock* TitleText = MakeScreenText(TEXT("FeatureFrameTitle"), 30, FLinearColor(0.96f, 0.94f, 0.88f, 1.0f), true, ETextJustify::Center);
	TitleText->SetText(Title);
	if (SourceTitlePixels > 0)
	{
		TitleText->SetFont(URokMainHudWidget::MakeUnitySourceFont(SourceTitlePixels, false));
		TitleText->SetShadowOffset(FVector2D::ZeroVector);
		TitleText->SetShadowColorAndOpacity(FLinearColor::Transparent);
	}
	const float TitleOffset = SourceTitlePixels > 0 ? 12.0f : 8.0f;
	PlaceOnCanvas(Parent, TitleText, FVector2D(Position.X, Position.Y + TitleOffset), FVector2D(Size.X, 42.0f), 7);
	PlaceOnCanvas(Parent, MakeActionButton(TEXT("FeatureFrameClose"), CloseAction, CommonAtlasPath, CloseRect, CommonAtlasSize, FVector2D(69.0f)), ClosePosition, FVector2D(69.0f), 10);
}

void URokUnityFeatureScreenWidget::BuildResearch()
{
	const TSubclassOf<URokResearchScreenWidget> Presentation = URokResearchScreenWidget::LoadPresentationClass();
	if (!Presentation)
	{
		UE_LOG(LogTemp, Error, TEXT("Rok research presentation Widget Blueprint is missing."));
		return;
	}
	ResearchScreenWidget = WidgetTree->ConstructWidget<URokResearchScreenWidget>(Presentation, TEXT("ResearchScreenRoot"));
	if (!ResearchScreenWidget || !ResearchScreenWidget->InitializeResearch(FeatureContextActor.Get()))
	{
		UE_LOG(LogTemp, Error, TEXT("Rok research presentation failed to initialize."));
		ResearchScreenWidget = nullptr;
		return;
	}
	ResearchScreenWidget->OnCloseRequested.AddUObject(
		this, &URokUnityFeatureScreenWidget::RequestCloseScreen);
	ResearchScreenWidget->OnDetailVisibilityChanged.AddUObject(
		this, &URokUnityFeatureScreenWidget::HandleResearchDetailVisibilityChanged);
	ResearchScreenWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ModeCanvas, ResearchScreenWidget, FVector2D::ZeroVector,
		FVector2D(1440.0f, 810.0f), 3);

}
void URokUnityFeatureScreenWidget::HandleResearchDetailVisibilityChanged(bool bVisible)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Research || !WidgetTree) return;
	// UIManager.ShowLastMask keeps only the top PopWin mask active.
	if (UWidget* Dimmer = GetWidgetFromName(TEXT("FeatureScreenDimmer")))
		Dimmer->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

#include "RokFeatureMailReference.inl"

void URokUnityFeatureScreenWidget::BuildMail()
{
	FRokMailRewardTemplate MailTemplate;
	FString MailTemplateError;
	const bool bHasMailTemplate = bReferenceMailCaptureState
		&& RokMailRewardDetails::LoadTemplate(MailTemplate, MailTemplateError);
	if (bReferenceMailCaptureState && !bHasMailTemplate)
	{
		UE_LOG(LogTemp, Error, TEXT("Rok mail source template: %s"), *MailTemplateError);
	}
	RenderedMailRewardRows.Reset();
	if (SelectedMailTab == 0 && bReferenceMailCaptureState && SelectedMailIndex != 1)
	{
		const FRokMailRewardPayload* Payload = MailRewardFixturePayloadByIndex.Find(SelectedMailIndex);
		if (!Payload && !bReadOnlyMailRewardFixtureState)
		{
			Payload = MailRewardFixturePayloadByIndex.Find(0);
		}
		if (Payload)
		{
			RenderedMailRewardRows = RokMailRewardDetails::Normalize(
				*Payload, GetEconomyCatalogSubsystem(), GetGameplayCatalogSubsystem());
		}
	}
	else if (SelectedMailTab == 0 && bReadOnlyMailRewardFixtureState)
	{
		if (const FRokMailRewardPayload* Payload = MailRewardFixturePayloadByIndex.Find(SelectedMailIndex))
		{
			RenderedMailRewardRows = RokMailRewardDetails::Normalize(
				*Payload, GetEconomyCatalogSubsystem(), GetGameplayCatalogSubsystem());
		}
	}
	if (bReferenceMailCaptureState && TryBuildMailReferencePage(MailTemplate, bHasMailTemplate, MailTemplateError))
	{
		return;
	}
	const bool bLocalTabHasMail = bLocalMailUnreadFixtureState
		&& MailLocalUnreadState.FirstIndexForTab(SelectedMailTab) != INDEX_NONE;
	const bool bUsesCommonMailPage = !bReferenceMailCaptureState && !bReadOnlyMailRewardFixtureState;
	const bool bReportTab = bUsesCommonMailPage && !bLocalMailUnreadFixtureState && SelectedMailTab == 1;
	if (bUsesCommonMailPage)
	{
		UClass* RowClass = bReportTab ? nullptr : ResolveFeaturePageClass(TEXT("MailLocalRow"));
		UClass* SelectedRowClass = bReportTab ? nullptr : ResolveFeaturePageClass(TEXT("MailLocalRowSelected"));
		auto* Page = bReportTab || (RowClass && SelectedRowClass) ? MountModePage(TEXT("MailLocal")) : nullptr;
		UScrollBox* List = Page ? Page->Get<UScrollBox>(TEXT("MailListScroll")) : nullptr;
		if (Page && !List)
		{
			Page->RemoveFromParent();
			ActiveModePage = nullptr;
		}
		if (Page && List)
		{
			BindMailPageControls(*Page);
			for (int32 Tab = 0; Tab < 6; ++Tab)
			{
				const int32 Unread = bLocalMailUnreadFixtureState ? MailLocalUnreadState.UnreadCount(Tab) : GetSessionMailUnreadCount(Tab);
				UWidget* Dot = Page->Get<UImage>(*FString::Printf(TEXT("MailLocalTabRedPoint_%d"), Tab));
				UTextBlock* Count = Page->Get<UTextBlock>(*FString::Printf(TEXT("MailLocalTabUnreadCount_%d"), Tab));
				Page->SetPresent(Dot, Unread > 0);
				Page->SetPresent(Count, Unread > 0);
				if (Unread > 0 && Count)
				{
					Count->SetText(FText::AsNumber(Unread));
				}
			}
			Page->SetPresent(Page->Get<UTextBlock>(TEXT("MailLocalUnreadNotice")), bLocalMailUnreadFixtureState);
			Page->SetPresent(Page->Get<UTextBlock>(TEXT("MailEmptyState")), !bReportTab && !bLocalTabHasMail);
			Page->SetPresent(Page->Get<UCanvasPanel>(TEXT("MailDetailPanel")), bLocalTabHasMail);
			UTextBlock* DetailTitle = Page->Get<UTextBlock>(TEXT("MailLocalDetailTitle"));
			UTextBlock* DetailBody = Page->Get<UTextBlock>(TEXT("MailLocalDetailBody"));
			Page->SetPresent(DetailTitle, bLocalTabHasMail);
			Page->SetPresent(DetailBody, bLocalTabHasMail);
			if (bReportTab)
			{
				BuildBattleReportMail(List);
				return;
			}
			if (!bLocalMailUnreadFixtureState)
			{
				Page->SetPresent(Page->Get<UTextBlock>(TEXT("MailEmptyState")), false);
				BuildSessionMail(List);
				return;
			}
			int32 VisibleCount = 0;
			if (bLocalTabHasMail)
			{
				for (int32 Index = 0; Index < MailLocalUnreadState.Rows.Num(); ++Index)
				{
					const FRokMailLocalUnreadRow& LocalRow = MailLocalUnreadState.Rows[Index];
					if (LocalRow.Tab != SelectedMailTab)
					{
						continue;
					}
					UClass* StyleClass = Index == SelectedMailIndex ? SelectedRowClass : RowClass;
					URokUiPageWidget* Row = CreateWidget<URokUiPageWidget>(this, StyleClass);
					if (!Row)
					{
						continue;
					}
					Row->LegacyPrefix = TEXT("Mail");
					Row->LegacySuffix = FString::Printf(TEXT("_%d"), Index);
					Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					List->AddChild(Row);
					SetFeaturePageText(*Row, TEXT("RowTitle"), FText::FromString(LocalRow.Title));
					SetFeaturePageText(*Row, TEXT("RowSubject"), FText::FromString(LocalRow.Summary));
					Row->SetPresent(Row->Get<UImage>(TEXT("Unread")), !LocalRow.bRead);
					BindPageAction(*Row, TEXT("RowHit"), 220 + Index);
					++VisibleCount;
				}
				const FRokMailLocalUnreadRow* Selected = MailLocalUnreadState.Find(SelectedMailIndex);
				if (DetailTitle)
				{
					DetailTitle->SetText(Selected ? FText::FromString(Selected->Title)
						: NSLOCTEXT("RokFeature", "MailLocalSelectRow", "请选择一封邮件"));
				}
				if (DetailBody)
				{
					DetailBody->SetText(Selected ? FText::FromString(Selected->Summary) : FText::GetEmpty());
				}
			}
			UE_LOG(LogTemp, Display, TEXT("Rok mail screen: source=UI_Win_Mail logical_page=%d rendered_rows=%d authority=%s."),
				SelectedMailTab, VisibleCount, bLocalMailUnreadFixtureState ? TEXT("explicit_local_unread_fixture") : TEXT("unbound"));
			return;
		}
	}

	// UI_Win_Mail owns four frame pieces over the full 1440x810 canvas.
	// Mail is the only current screen whose Unity source uses this full-size
	// placement, so keep the source coordinates instead of CapturePoint.
	UCanvasPanel* BodyLeft = MakeAtlasBorder(
		TEXT("MailFrameBodyLeft"), FrameAtlasPath, WindowBodyRect,
		FrameAtlasSize, FVector2D(690.0f, 758.0f), WindowBodyMargin);
	PlaceOnCanvas(ModeCanvas, BodyLeft, FVector2D(30.0f, 43.5f), FVector2D(690.0f, 758.0f), 1);
	UCanvasPanel* BodyRight = MakeAtlasBorder(
		TEXT("MailFrameBodyRight"), FrameAtlasPath, WindowBodyRect,
		FrameAtlasSize, FVector2D(690.0f, 758.0f), WindowBodyMargin);
	BodyRight->SetRenderTransformPivot(FVector2D(0.5f));
	BodyRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, BodyRight, FVector2D(720.0f, 43.5f), FVector2D(690.0f, 758.0f), 1);
	UCanvasPanel* HeaderLeft = MakeAtlasBorder(
		TEXT("MailFrameHeaderLeft"), FrameAtlasPath, WindowHeaderRect,
		FrameAtlasSize, FVector2D(724.0f, 60.0f), WindowHeaderMargin);
	PlaceOnCanvas(ModeCanvas, HeaderLeft, FVector2D(-4.0f, 14.0f), FVector2D(724.0f, 60.0f), 5);
	UCanvasPanel* HeaderRight = MakeAtlasBorder(
		TEXT("MailFrameHeaderRight"), FrameAtlasPath, WindowHeaderRect,
		FrameAtlasSize, FVector2D(724.0f, 60.0f), WindowHeaderMargin);
	HeaderRight->SetRenderTransformPivot(FVector2D(0.5f));
	HeaderRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, HeaderRight, FVector2D(720.0f, 14.0f), FVector2D(724.0f, 60.0f), 5);
	UTextBlock* MailTitle = MakeScreenText(
		TEXT("FeatureFrameTitle"), 30, FLinearColor::White, false,
		ETextJustify::Center);
	MailTitle->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 30.0f * 72.0f / 96.0f));
	MailTitle->SetShadowOffset(FVector2D(0.0f, 2.0f));
	MailTitle->SetShadowColorAndOpacity(FLinearColor::FromSRGBColor(FColor(29, 22, 22)));
	MailTitle->SetText(NSLOCTEXT("RokFeature", "MailTitle", "邮件"));
	PlaceOnCanvas(ModeCanvas, MailTitle, FVector2D(139.5f, 20.0f), FVector2D(1161.0f, 40.0f), 7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("FeatureFrameClose"), 1, CommonAtlasPath,
			CloseRect, CommonAtlasSize, FVector2D(69.0f)),
		FVector2D(1364.0f, 2.0f), FVector2D(69.0f), 10);

	static const FText TabLabels[] = {
		NSLOCTEXT("RokFeature", "MailSystem", "系统"),
		NSLOCTEXT("RokFeature", "MailReport", "报告"),
		NSLOCTEXT("RokFeature", "MailAlliance", "联盟"),
		NSLOCTEXT("RokFeature", "MailPersonal", "个人"),
		NSLOCTEXT("RokFeature", "MailSent", "已发送"),
		NSLOCTEXT("RokFeature", "MailFavorite", "收藏")};
	// Chinese ArabLayoutCompment mirrors the source page strip at runtime.
	// Logical page order remains System, Report, Guild, Person, Sent, Collect.
	static const float TabX[] = {87.8f, 269.8f, 451.8f, 815.8f, 997.8f, 633.8f};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const bool bSelected = Index == SelectedMailTab;
		URokUnityIndexedButton* TabButton = MakeActionButton(
				*FString::Printf(TEXT("MailTab_%d"), Index), 200 + Index,
				CommonAtlasPath, bSelected ? PageSelectedRect : PageNormalRect,
				CommonAtlasSize, FVector2D(182.0f, 45.0f), TabLabels[Index], 17,
				bSelected ? FLinearColor::White
					: FLinearColor::FromSRGBColor(FColor(164, 157, 146)),
				ESlateBrushDrawType::Box,
				FMargin(0.42f, 0.0f, 0.42f, 0.0f));
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(TabButton->GetContentSlot()))
		{
			ContentSlot->SetPadding(FMargin(0.0f));
		}
		PlaceOnCanvas(ModeCanvas, TabButton,
			FVector2D(TabX[Index], 77.95f), FVector2D(182.0f, 45.0f), 8);
		if (bLocalMailUnreadFixtureState && MailLocalUnreadState.UnreadCount(Index) > 0)
		{
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(
				*FString::Printf(TEXT("MailLocalTabRedPoint_%d"), Index), CommonAtlasPath,
				RedPointRect, CommonAtlasSize, FVector2D(27.0f, 29.0f)),
				FVector2D(TabX[Index] + 170.0f, 68.0f), FVector2D(27.0f, 29.0f), 10);
			UTextBlock* Count = MakeScreenText(
				*FString::Printf(TEXT("MailLocalTabUnreadCount_%d"), Index), 14,
				FLinearColor::White, true, ETextJustify::Center);
			Count->SetText(FText::AsNumber(MailLocalUnreadState.UnreadCount(Index)));
			PlaceOnCanvas(ModeCanvas, Count,
				FVector2D(TabX[Index] + 170.0f, 70.0f), FVector2D(27.0f, 22.0f), 11);
		}
		const FName TabLabelName(*FString::Printf(TEXT("MailTab_%dLabel"), Index));
		if (UTextBlock* TabLabel = Cast<UTextBlock>(GetWidgetFromName(TabLabelName)))
		{
			TabLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 22.0f * 72.0f / 96.0f));
			TabLabel->SetShadowOffset(FVector2D::ZeroVector);
			if (UCanvasPanel* TabContent = Cast<UCanvasPanel>(TabButton->GetContent()))
			{
				TabLabel->RemoveFromParent();
				UBorder* LabelRegion = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
					*FString::Printf(TEXT("MailTab_%dLabelRegion"), Index));
				LabelRegion->SetBrushColor(FLinearColor::Transparent);
				LabelRegion->SetPadding(FMargin(0.0f));
				LabelRegion->SetHorizontalAlignment(HAlign_Fill);
				LabelRegion->SetVerticalAlignment(VAlign_Center);
				LabelRegion->SetVisibility(ESlateVisibility::HitTestInvisible);
				LabelRegion->SetContent(TabLabel);
				PlaceOnCanvas(TabContent, LabelRegion, FVector2D(0.0f, 1.1f),
					FVector2D(182.0f, 45.0f), 2);
			}
		}
	}

	// Unity keeps the compose entry visible even when the current category has
	// zero rows. The action remains behind the server boundary in UE.
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("MailCompose"), 241, nullptr, FIntRect(),
			FIntPoint(1, 1), FVector2D(175.0f, 60.0f)),
		FVector2D(1189.95f, 65.45f), FVector2D(175.0f, 60.0f), 8);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(TEXT("MailComposeIcon"), MailAtlasPath,
			SpriteRect(1611, 507, 27, 23), MailAtlasSize,
			FVector2D(27.0f, 23.0f)),
		FVector2D(1212.55f, 83.95f), FVector2D(27.0f, 23.0f), 9);
	UTextBlock* ComposeLabel = MakeScreenText(
		TEXT("MailComposeLabel"), 17,
		FLinearColor::White, false,
		ETextJustify::Center);
	ComposeLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 22.0f * 72.0f / 96.0f));
	ComposeLabel->SetShadowOffset(FVector2D::ZeroVector);
	ComposeLabel->SetText(NSLOCTEXT("RokFeature", "NewMail", "新邮件"));
	PlaceOnCanvas(ModeCanvas, ComposeLabel, FVector2D(1251.95f, 65.45f), FVector2D(111.0f, 60.0f), 9);

	const bool bHasReferenceMail = bReferenceMailCaptureState && SelectedMailTab == 0;
	if (bReferenceMailCaptureState)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(TEXT("MailSystemTabRedPoint"), CommonAtlasPath,
				RedPointRect, CommonAtlasSize, FVector2D(27.0f, 29.0f)),
			FVector2D(257.8f, 68.0f), FVector2D(27.0f, 29.0f), 10);
		UTextBlock* SystemUnreadCount = MakeScreenText(
			TEXT("MailSystemTabUnreadCount"), 14, FLinearColor::White,
			true, ETextJustify::Center);
		SystemUnreadCount->SetText(FText::AsNumber(8));
		PlaceOnCanvas(ModeCanvas, SystemUnreadCount, FVector2D(257.8f, 70.0f), FVector2D(27.0f, 22.0f), 11);
	}

	// Chinese ArabLayoutCompment places the summary list left and selected mail
	// right in the running Unity window.
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(TEXT("MailListPanel"), CommonAtlasPath,
			CommonPanelRect, CommonAtlasSize, FVector2D(478.5f, 590.0f),
			CommonPanelMargin),
		FVector2D(56.0f, 128.0f), FVector2D(478.5f, 590.0f), 3);
	UScrollBox* MailListScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), TEXT("MailListScroll"));
	MailListScroll->SetOrientation(Orient_Vertical);
	MailListScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	MailListScroll->SetAllowOverscroll(true);
	MailListScroll->SetAnimateWheelScrolling(true);
	MailListScroll->SetWheelScrollMultiplier(12.0f);
	PlaceOnCanvas(ModeCanvas, MailListScroll, FVector2D(56.0f, 128.0f), FVector2D(478.5f, 590.0f), 5);
	if (bReportTab)
	{
		BuildBattleReportMail(MailListScroll);
		return;
	}
	if (bUsesCommonMailPage && !bLocalMailUnreadFixtureState)
	{
		BuildSessionMail(MailListScroll);
		return;
	}
	if (bLocalTabHasMail)
	{
		for (int32 Index = 0; Index < MailLocalUnreadState.Rows.Num(); ++Index)
		{
			if (MailLocalUnreadState.Rows[Index].Tab != SelectedMailTab) continue;
			UCanvasPanel* RowContainer = WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(), *FString::Printf(TEXT("MailRowContainer_%d"), Index));
			PlaceOnCanvas(RowContainer, MakeMailRow(Index, nullptr),
				FVector2D(10.75f, 0.0f), FVector2D(457.0f, 114.24f), 1);
			MailListScroll->AddChild(WrapFixedSize(RowContainer,
				*FString::Printf(TEXT("MailRowSize_%d"), Index), FVector2D(478.5f, 114.0f)));
		}
		PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("MailDetailPanel"), CommonAtlasPath,
			CommonPanelRect, CommonAtlasSize, FVector2D(830.0f, 590.0f), CommonPanelMargin),
			FVector2D(547.0f, 128.0f), FVector2D(830.0f, 590.0f), 3);
		const FRokMailLocalUnreadRow* Selected = MailLocalUnreadState.Find(SelectedMailIndex);
		UTextBlock* DetailTitle = MakeScreenText(TEXT("MailLocalDetailTitle"), 24, FLinearColor::White, true);
		DetailTitle->SetText(Selected ? FText::FromString(Selected->Title)
			: NSLOCTEXT("RokFeature", "MailLocalSelectRow", "请选择一封邮件"));
		PlaceOnCanvas(ModeCanvas, DetailTitle, FVector2D(570.0f, 158.0f), FVector2D(775.0f, 45.0f), 6);
		UTextBlock* DetailBody = MakeScreenText(TEXT("MailLocalDetailBody"), 18, FLinearColor::White);
		DetailBody->SetText(Selected ? FText::FromString(Selected->Summary) : FText::GetEmpty());
		PlaceOnCanvas(ModeCanvas, DetailBody, FVector2D(570.0f, 240.0f), FVector2D(775.0f, 45.0f), 6);
	}
	else if (bHasReferenceMail)
	{
		for (int32 Index = 0; Index < 6; ++Index)
		{
			UCanvasPanel* RowContainer = WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(),
				*FString::Printf(TEXT("MailRowContainer_%d"), Index));
			PlaceOnCanvas(RowContainer, MakeMailRow(Index, bHasMailTemplate ? &MailTemplate : nullptr),
				FVector2D(10.75f, 0.0f), FVector2D(457.0f, 114.24f), 1);
			MailListScroll->AddChild(WrapFixedSize(
				RowContainer,
				*FString::Printf(TEXT("MailRowSize_%d"), Index),
				FVector2D(478.5f, 114.0f)));
		}

		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasBorder(TEXT("MailDetailPanel"), CommonAtlasPath,
				CommonPanelRect, CommonAtlasSize, FVector2D(830.0f, 590.0f),
				CommonPanelMargin),
			FVector2D(547.0f, 128.0f), FVector2D(830.0f, 590.0f), 3);
		const int32 GiftRowCount = FMath::DivideAndRoundUp(RenderedMailRewardRows.Num(), 5);
		const float GiftHeight = GiftRowCount > 0 ? 140.0f + (GiftRowCount - 1) * 170.0f : 0.0f;
		const float ClaimY = 409.0f + GiftHeight;
		const float DetailContentHeight = FMath::Max(590.0f, ClaimY + 84.0f - 128.0f);
		UScrollBox* MailDetailScroll = WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(), TEXT("MailDetailScroll"));
		MailDetailScroll->SetOrientation(Orient_Vertical);
		MailDetailScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		MailDetailScroll->SetAllowOverscroll(false);
		PlaceOnCanvas(ModeCanvas, MailDetailScroll, FVector2D(547.0f, 128.0f), FVector2D(830.0f, 590.0f), 5);
		UCanvasPanel* DetailContent = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("MailDetailContent"));
		MailDetailScroll->AddChild(WrapFixedSize(DetailContent, TEXT("MailDetailContentSize"),
			FVector2D(830.0f, DetailContentHeight)));
		auto PlaceDetail = [this, DetailContent](UWidget* Child, FVector2D AbsolutePosition,
			FVector2D Size, int32 ZOrder)
		{
			PlaceOnCanvas(DetailContent, Child, AbsolutePosition - FVector2D(547.0f, 128.0f), Size, ZOrder);
		};
		UImage* DetailPoster = MakeAtlasImage(TEXT("MailDetailPoster"), MailAtlasPath,
			SpriteRect(824, 650, 821, 123), MailAtlasSize,
			FVector2D(821.0f, 123.0f));
		// Chinese ArabLayoutCompment mirrors this poster without moving its rect.
		DetailPoster->SetRenderTransformPivot(FVector2D(0.5f));
		DetailPoster->SetRenderScale(FVector2D(-1.0f, 1.0f));
		PlaceDetail(DetailPoster, FVector2D(551.5f, 127.0f), FVector2D(821.0f, 123.0f), 4);
		UTextBlock* DetailTitle = MakeScreenText(TEXT("MailDetailTitle"), 20, FLinearColor::White, true);
		DetailTitle->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 26.0f * 72.0f / 96.0f));
		DetailTitle->SetShadowOffset(FVector2D::ZeroVector);
		DetailTitle->SetText(bHasMailTemplate ? FText::FromString(MailTemplate.Title) : FText::GetEmpty());
		PlaceDetail(DetailTitle, FVector2D(566.0f, 134.9f), FVector2D(782.5f, 36.7f), 6);
		UTextBlock* DetailSub = MakeScreenText(TEXT("MailDetailSub"), 15, FLinearColor::White);
		DetailSub->SetShadowOffset(FVector2D::ZeroVector);
		DetailSub->SetText(SelectedMailIndex == 1
			? NSLOCTEXT("RokFeature", "MailBanquetDetailSubject", "英豪盛宴活动奖励补发")
			: (bHasMailTemplate ? FText::FromString(MailTemplate.Subtitle) : FText::GetEmpty()));
		PlaceDetail(DetailSub, FVector2D(566.0f, 178.0f), FVector2D(782.5f, 43.3f), 6);

		if (SelectedMailIndex == 1 && RenderedMailRewardRows.IsEmpty())
		{
			UTextBlock* EmptyBody = MakeScreenText(TEXT("MailDetailUnavailable"), 19, FLinearColor(0.9f, 0.9f, 0.88f, 1.0f));
			EmptyBody->SetText(bReadOnlyMailRewardFixtureState
				? NSLOCTEXT("RokFeature", "MailBanquetFixtureBodyUnavailable", "该参考邮件正文尚未迁入本地预览。")
				: NSLOCTEXT("RokFeature", "MailBanquetUnavailable", "该邮件正文与附件尚未迁入本地预览。"));
			PlaceDetail(EmptyBody, FVector2D(569.0f, 273.0f), FVector2D(785.0f, 42.0f), 6);
			if (bReadOnlyMailRewardFixtureState) BuildMailRewards(DetailContent);
		}
		else
		{
			FString SuffixLine = bHasMailTemplate ? MailTemplate.BodySuffix : FString();
			FString FinalLineText;
			if (SuffixLine.Len() >= 4)
			{
				// Native 1440x810 TextGenerator wraps only the final four glyphs.
				FinalLineText = SuffixLine.Right(4);
				SuffixLine.LeftChopInline(4);
			}
			UTextBlock* Greeting = MakeScreenText(TEXT("MailDetailGreeting"), 15, FLinearColor::White);
			Greeting->SetShadowOffset(FVector2D::ZeroVector);
			Greeting->SetText(FText::FromString(bHasMailTemplate ? MailTemplate.BodyGreeting : MailTemplateError));
			PlaceDetail(Greeting, FVector2D(569.0f, 273.0f), FVector2D(785.0f, 22.0f), 6);
			UTextBlock* Prefix = MakeScreenText(TEXT("MailDetailPrefix"), 15, FLinearColor::White);
			Prefix->SetShadowOffset(FVector2D::ZeroVector);
			Prefix->SetText(FText::FromString(MailTemplate.BodyPrefix));
			PlaceDetail(Prefix, FVector2D(569.0f, 296.0f), FVector2D(40.0f, 22.0f), 6);
			UTextBlock* Highlight = MakeScreenText(TEXT("MailDetailHighlight"), 15,
				FLinearColor::FromSRGBColor(FColor(0, 128, 0)), true);
			Highlight->SetShadowOffset(FVector2D::ZeroVector);
			Highlight->SetText(FText::FromString(MailTemplate.BodyActivityQuoted));
			PlaceDetail(Highlight, FVector2D(609.0f, 296.0f), FVector2D(191.0f, 22.0f), 6);
			UTextBlock* Suffix = MakeScreenText(TEXT("MailDetailSuffix"), 15, FLinearColor::White);
			Suffix->SetShadowOffset(FVector2D::ZeroVector);
			Suffix->SetText(FText::FromString(SuffixLine));
			PlaceDetail(Suffix, FVector2D(800.0f, 296.0f), FVector2D(555.0f, 22.0f), 6);
			UTextBlock* FinalLine = MakeScreenText(TEXT("MailDetailFinalLine"), 15, FLinearColor::White);
			FinalLine->SetShadowOffset(FVector2D::ZeroVector);
			FinalLine->SetText(FText::FromString(FinalLineText));
			PlaceDetail(FinalLine, FVector2D(569.0f, 319.0f), FVector2D(785.0f, 22.0f), 6);

			BuildMailRewards(DetailContent);
		}
		if (SelectedMailIndex != 1 || bReadOnlyMailRewardFixtureState)
		{
			PlaceDetail(
				MakeActionButton(TEXT("MailClaim"), 240, CommonAtlasPath,
					MediumBlueButtonRect, CommonAtlasSize,
					FVector2D(202.0f, 84.0f),
					FText::GetEmpty(), 24,
					FLinearColor(0.02f, 0.17f, 0.29f, 1.0f)),
				FVector2D(861.0f, ClaimY), FVector2D(202.0f, 84.0f), 7);
			UTextBlock* ClaimLabel = MakeScreenText(TEXT("MailClaimLabel"), 24,
				FLinearColor::FromSRGBColor(FColor(10, 27, 46)), true, ETextJustify::Center);
			// Unity's 32px BestFit label uses 30px in the 185.4x34 source rect.
			ClaimLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30.0f * 72.0f / 96.0f));
			ClaimLabel->SetShadowOffset(FVector2D::ZeroVector);
			ClaimLabel->SetText(FText::FromString(MailTemplate.ClaimLabel));
			PlaceDetail(ClaimLabel, FVector2D(869.3f, ClaimY + 25.0f),
				FVector2D(185.4f, 34.0f), 8);
			BuildMailRewardTooltip();
		}
	}
	else
	{
		UTextBlock* Empty = MakeScreenText(
			TEXT("MailEmptyState"), 24, FLinearColor::White, true,
			ETextJustify::Center);
		Empty->SetText(NSLOCTEXT("RokFeature", "MailEmptyState", "当前暂无邮件。"));
		PlaceOnCanvas(ModeCanvas, Empty, FVector2D(547.0f, 128.0f), FVector2D(830.0f, 590.0f), 6);
	}
	if (bLocalMailUnreadFixtureState)
	{
		UTextBlock* LocalNotice = MakeScreenText(TEXT("MailLocalUnreadNotice"), 14,
			FLinearColor(0.90f, 0.67f, 0.30f, 1.0f), true, ETextJustify::Center);
		LocalNotice->SetText(NSLOCTEXT("RokFeature", "MailLocalUnreadNotice", "本地邮件交互夹具 · 不是当前账号邮件"));
		PlaceOnCanvas(ModeCanvas, LocalNotice, FVector2D(470.0f, 785.0f), FVector2D(500.0f, 22.0f), 12);
	}

	if (bReferenceMailCaptureState)
	{
		UTextBlock* ReferenceNotice = MakeScreenText(
			TEXT("MailReferencePreviewNotice"), 14,
			FLinearColor(0.90f, 0.67f, 0.30f, 1.0f), true,
			ETextJustify::Center);
		ReferenceNotice->SetText(bReadOnlyMailRewardFixtureState
			? NSLOCTEXT("RokFeature", "MailRewardFixtureNotice", "只读奖励夹具 · 不是当前账号邮件")
			: NSLOCTEXT("RokFeature", "MailReferencePreviewNotice", "Unity 截图参考态 · 不是当前账号邮件"));
		PlaceOnCanvas(ModeCanvas, ReferenceNotice, FVector2D(470.0f, 785.0f), FVector2D(500.0f, 22.0f), 12);
	}

	if (bHasReferenceMail)
	{
		static const FText FooterLabels[] = {
			NSLOCTEXT("RokFeature", "MailDeleteRead", "删除已读"),
			NSLOCTEXT("RokFeature", "MailReadClaimAll", "一键已读&领取"),
			NSLOCTEXT("RokFeature", "MailFavoriteAction", "收藏"),
			NSLOCTEXT("RokFeature", "MailDelete", "删除")};
		static const FIntRect FooterIcons[] = {
			SpriteRect(1900, 565, 25, 29), SpriteRect(807, 495, 27, 28),
			SpriteRect(226, 22, 28, 26), SpriteRect(1900, 565, 25, 29)};
		// The source right-side HorizontalLayoutGroup closes the hidden Reply and
		// Forward slots, then lays out the active Collect and Delete buttons.
		static const float FooterX[] = {55.04f, 281.06f, 780.61f, 979.62f};
		static const float FooterLabelWidth[] = {168.71f, 204.12f, 139.10f, 139.10f};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			UCanvasPanel* Footer = WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(),
				*FString::Printf(TEXT("MailFooter_%d"), Index));
			PlaceOnCanvas(Footer, MakeAtlasImage(
				*FString::Printf(TEXT("MailFooterIcon_%d"), Index),
				MailAtlasPath, FooterIcons[Index], MailAtlasSize,
				FVector2D(30.0f)), FVector2D(5.0f, 8.0f), FVector2D(30.0f), 2);
			UTextBlock* FooterText = MakeScreenText(
				*FString::Printf(TEXT("MailFooterLabel_%d"), Index), 17,
				FLinearColor::White);
			FooterText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 22.0f * 72.0f / 96.0f));
			FooterText->SetShadowOffset(FVector2D::ZeroVector);
			FooterText->SetText(FooterLabels[Index]);
			UBorder* FooterLabelRegion = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), *FString::Printf(TEXT("MailFooterLabel_%dRegion"), Index));
			FooterLabelRegion->SetBrushColor(FLinearColor::Transparent);
			FooterLabelRegion->SetPadding(FMargin(0.0f));
			FooterLabelRegion->SetHorizontalAlignment(HAlign_Fill);
			FooterLabelRegion->SetVerticalAlignment(VAlign_Center);
			FooterLabelRegion->SetVisibility(ESlateVisibility::HitTestInvisible);
			FooterLabelRegion->SetContent(FooterText);
			PlaceOnCanvas(Footer, FooterLabelRegion, FVector2D(50.26f, -6.50f),
				FVector2D(FooterLabelWidth[Index], 60.0f), 2);
			PlaceOnCanvas(Footer, MakeActionButton(
				*FString::Printf(TEXT("MailFooterHit_%d"), Index),
				250 + Index, nullptr, FIntRect(), FIntPoint(1, 1),
				FVector2D(175.0f, 47.0f)),
				FVector2D::ZeroVector, FVector2D(175.0f, 47.0f), 5);
			PlaceOnCanvas(ModeCanvas, Footer, FVector2D(FooterX[Index], 731.98f), FVector2D(175.0f, 47.0f), 7);
		}
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok mail screen: source=UI_Win_Mail logical_page=%d rendered_rows=%d authority=%s."),
		SelectedMailTab,
		bLocalMailUnreadFixtureState ? MailLocalUnreadState.Rows.FilterByPredicate(
			[this](const FRokMailLocalUnreadRow& Row) { return Row.Tab == SelectedMailTab; }).Num()
			: (bHasReferenceMail ? 6 : 0),
		bLocalMailUnreadFixtureState ? TEXT("explicit_local_unread_fixture")
			: (bReadOnlyMailRewardFixtureState ? TEXT("read_only_reward_fixture")
			: (bReferenceMailCaptureState ? TEXT("reference_fixture") : TEXT("unbound"))));
}

bool URokUnityFeatureScreenWidget::ResolveMailRewardIcon(
	const FRokMailRewardRow& RewardRow,
	const TCHAR*& OutAtlasPath,
	FIntRect& OutRect,
	FIntPoint& OutAtlasSize) const
{
	OutAtlasPath = nullptr;
	if (RewardRow.Kind == ERokMailRewardKind::Currency)
	{
		// Currency.csv iconID resolves to these ui_common sprite metadata rects.
		static const FIntRect CurrencyRects[] = {
			SpriteRect(865, 349, 80, 80), SpriteRect(1085, 489, 80, 80),
			SpriteRect(947, 431, 80, 80), SpriteRect(865, 431, 80, 80),
			SpriteRect(783, 435, 80, 80)};
		const int64 CurrencyIndex = RewardRow.Id - 100;
		if (CurrencyIndex < 0 || CurrencyIndex >= UE_ARRAY_COUNT(CurrencyRects)) return false;
		OutAtlasPath = CommonAtlasPath;
		OutRect = CurrencyRects[CurrencyIndex];
		OutAtlasSize = CommonAtlasSize;
		return true;
	}
	if (RewardRow.Kind == ERokMailRewardKind::Soldier)
	{
		if (!ResolveTroopHeadRect(RewardRow.TroopArmsType, RewardRow.TroopLevel, OutRect)) return false;
		OutAtlasPath = TroopHeadAtlasPath;
		OutAtlasSize = TroopHeadAtlasSize;
		return true;
	}
	if (RewardRow.Kind == ERokMailRewardKind::Item)
	{
		const URokUnityEconomyCatalogSubsystem* Catalog = GetEconomyCatalogSubsystem();
		if (!Catalog) return false;
		const TArray<FRokUnityEconomyItem> Items = Catalog->GetItems();
		const FRokUnityEconomyItem* Item = Items.FindByPredicate(
			[&RewardRow](const FRokUnityEconomyItem& Candidate)
			{
				return Candidate.Id == RewardRow.Id && Candidate.Icon == RewardRow.Icon;
			});
		return Item && ResolveEconomyItemIcon(*Item, OutAtlasPath, OutRect, OutAtlasSize);
	}
	return false;
}

void URokUnityFeatureScreenWidget::BuildMailRewards(UCanvasPanel* DetailContent)
{
	if (RenderedMailRewardRows.IsEmpty())
	{
		if (bReadOnlyMailRewardFixtureState
			&& MailRewardFixturePayloadByIndex.Contains(SelectedMailIndex))
		{
			// A non-null source RewardInfo with no entries leaves pl_gift at height 0.
			UCanvasPanel* EmptyGrid = WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(), TEXT("MailRewardFixtureGrid"));
			PlaceOnCanvas(DetailContent, EmptyGrid, FVector2D(5.0f, 237.0f),
				FVector2D(820.0f, 0.0f), 6);
			return;
		}
		UTextBlock* Empty = MakeScreenText(TEXT("MailRewardFixtureEmpty"), 17,
			FLinearColor(0.9f, 0.9f, 0.88f, 1.0f));
		Empty->SetText(MailRewardFixtureError.IsEmpty()
			? NSLOCTEXT("RokFeature", "MailNoRewardPayload", "该邮件没有附件数据。")
			: FText::FromString(MailRewardFixtureError));
		PlaceOnCanvas(DetailContent, Empty, FVector2D(22.0f, 237.0f), FVector2D(785.0f, 32.0f), 6);
		return;
	}

	const bool bLegacyReference = !bReadOnlyMailRewardFixtureState && RenderedMailRewardRows.Num() == 1;
	UClass* ReferenceCardClass = ActiveModePage && bLegacyReference
		? ResolveFeaturePageClass(TEXT("MailRewardReferenceCard")) : nullptr;
	UClass* ItemCardClass = ActiveModePage && !bLegacyReference
		? ResolveFeaturePageClass(TEXT("MailRewardItemCard")) : nullptr;
	UClass* CurrencyCardClass = ActiveModePage && !bLegacyReference
		? ResolveFeaturePageClass(TEXT("MailRewardCurrencyCard")) : nullptr;
	UCanvasPanel* RewardGrid = nullptr;
	if (!bLegacyReference)
	{
		RewardGrid = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MailRewardFixtureGrid"));
		const int32 GridRows = FMath::DivideAndRoundUp(RenderedMailRewardRows.Num(), 5);
		PlaceOnCanvas(DetailContent, RewardGrid, FVector2D(5.0f, 237.0f),
			FVector2D(820.0f, 140.0f + (GridRows - 1) * 170.0f), 6);
	}

	for (int32 Index = 0; Index < RenderedMailRewardRows.Num(); ++Index)
	{
		const FRokMailRewardRow& Row = RenderedMailRewardRows[Index];
		const FName RootName(bLegacyReference ? TEXT("MailRewardItem")
			: *FString::Printf(TEXT("MailRewardFixtureRoot_%d"), Index));
		const float QualitySize = 93.8f;
		const FVector2D RootSize(130.0f, 140.0f);
		UClass* CardClass = bLegacyReference ? ReferenceCardClass
			: Row.Kind == ERokMailRewardKind::Currency ? CurrencyCardClass : ItemCardClass;
		if (CardClass)
		{
			if (auto* Card = CreateWidget<URokUiPageWidget>(this, CardClass))
			{
				BindMailRewardCard(*Card, Row, Index, bLegacyReference);
				PlaceOnCanvas(bLegacyReference ? DetailContent : RewardGrid, Card,
					bLegacyReference ? FVector2D(25.0f, 237.0f) : FVector2D(20.0f + (Index % 5) * 150.0f, (Index / 5) * 170.0f),
					RootSize, bLegacyReference ? 6 : 1);
				continue;
			}
		}
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), RootName);
		const FName QualityName(bLegacyReference ? TEXT("MailRewardItemQuality")
			: *FString::Printf(TEXT("MailRewardFixtureQuality_%d"), Index));
		UImage* Quality = MakeAtlasImage(QualityName, Item0AtlasPath,
			ResolveBagQualityRect(Row.Quality), Item0AtlasSize, FVector2D(QualitySize));
		if (Row.Kind != ERokMailRewardKind::Item || Row.Quality < 1 || Row.Quality > 5)
		{
			Quality->SetVisibility(ESlateVisibility::Collapsed);
		}
		PlaceOnCanvas(Root, Quality, FVector2D(18.1f, 2.8f), FVector2D(QualitySize), 1);
		const TCHAR* IconAtlasPath = nullptr;
		FIntRect IconRect;
		FIntPoint IconAtlasSize;
		const bool bIconResolved = ResolveMailRewardIcon(Row, IconAtlasPath, IconRect, IconAtlasSize);
		const FName IconName(bLegacyReference ? TEXT("MailRewardItemIcon")
			: *FString::Printf(TEXT("MailRewardFixtureIcon_%d"), Index));
		const FVector2D IconPosition = Row.Kind == ERokMailRewardKind::Currency
			? FVector2D(37.0f, 21.7f) : FVector2D(23.0f, 7.7f);
		const FVector2D IconSize = Row.Kind == ERokMailRewardKind::Currency
			? FVector2D(56.0f) : FVector2D(84.0f);
		PlaceOnCanvas(Root, MakeAtlasImage(IconName, bIconResolved ? IconAtlasPath : nullptr,
			IconRect, IconAtlasSize, IconSize), IconPosition, IconSize, 2);
		const FName QuantityName(bLegacyReference ? TEXT("MailRewardItemCount")
			: *FString::Printf(TEXT("MailRewardFixtureQuantity_%d"), Index));
		UTextBlock* Quantity = MakeScreenText(QuantityName, 20,
			Row.Kind == ERokMailRewardKind::Currency
				? FLinearColor::White
				: FLinearColor::FromSRGBColor(FColor(217, 212, 186)),
			true, ETextJustify::Right);
		// Both source gift branches inherit a 0.7 parent transform; their
		// 26-pixel prefab glyphs render at 18.2 pixels on this 1440x810 canvas.
		Quantity->SetFont(MailSourceQuantityFont());
		Quantity->SetShadowOffset(FVector2D::ZeroVector);
		Quantity->SetText(FText::FromString(Row.QuantityText));
		PlaceOnCanvas(Root, Quantity,
			Row.Kind == ERokMailRewardKind::Currency ? FVector2D(23.28f, 74.27f) : FVector2D(22.93f, 71.47f),
			FVector2D(80.22f, 21.0f), 4);
		if (Row.Kind != ERokMailRewardKind::Currency)
		{
			PlaceOnCanvas(Root, MakeActionButton(
				bLegacyReference ? TEXT("MailRewardItemHit")
					: *FString::Printf(TEXT("MailRewardFixtureHit_%d"), Index),
				2600 + Index, nullptr, FIntRect(), FIntPoint(1, 1),
					FVector2D(93.8f)),
				FVector2D(18.1f, 2.8f), FVector2D(93.8f), 5);
		}
		if (bLegacyReference)
		{
			PlaceOnCanvas(DetailContent, Root, FVector2D(25.0f, 237.0f), RootSize, 6);
			UTextBlock* Name = MakeScreenText(TEXT("MailRewardName"), 14,
				FLinearColor::White, false, ETextJustify::Center);
			Name->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18.0f * 72.0f / 96.0f));
			Name->SetShadowOffset(FVector2D::ZeroVector);
			Name->SetText(FText::FromString(Row.DisplayName));
			PlaceOnCanvas(DetailContent, Name, FVector2D(20.0f, 349.9f), FVector2D(140.0f, 46.1f), 6);
		}
		else
		{
			UTextBlock* Name = MakeScreenText(
				*FString::Printf(TEXT("MailRewardFixtureName_%d"), Index), 14,
				FLinearColor::White, false, ETextJustify::Center);
			Name->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18.0f * 72.0f / 96.0f));
			Name->SetShadowOffset(FVector2D::ZeroVector);
			Name->SetText(FText::FromString(Row.DisplayName));
			PlaceOnCanvas(Root, Name, FVector2D(-5.0f, 112.9f), FVector2D(140.0f, 46.1f), 4);
			const int32 Column = Index % 5;
			const int32 GridRow = Index / 5;
			PlaceOnCanvas(RewardGrid, Root,
				FVector2D(20.0f + Column * 150.0f, GridRow * 170.0f), RootSize, 1);
		}
	}
}

void URokUnityFeatureScreenWidget::ShowMailRewardTooltip(int32 RewardIndex)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Mail
		|| !RenderedMailRewardRows.IsValidIndex(RewardIndex)
		|| RenderedMailRewardRows[RewardIndex].Kind == ERokMailRewardKind::Currency)
	{
		return;
	}
	const FName RootName(!bReadOnlyMailRewardFixtureState && RenderedMailRewardRows.Num() == 1
		? TEXT("MailRewardItem")
		: *FString::Printf(TEXT("MailRewardFixtureRoot_%d"), RewardIndex));
	const UWidget* RewardWidget = GetWidgetFromName(RootName);
	if (!RewardWidget || !ModeCanvas) return;
	const FGeometry RewardGeometry = RewardWidget->GetCachedGeometry();
	const FGeometry ModeGeometry = ModeCanvas->GetCachedGeometry();
	MailRewardTooltipAnchor = ModeGeometry.AbsoluteToLocal(
		RewardGeometry.LocalToAbsolute(FVector2D(RewardGeometry.GetLocalSize().X * 0.5f, 0.0f)));
	ActiveMailRewardTooltipIndex = RewardIndex;
	BuildMailRewardTooltip();
}

void URokUnityFeatureScreenWidget::BuildMailRewardTooltip()
{
	if (MailRewardTooltipRoot)
	{
		MailRewardTooltipRoot->RemoveFromParent();
		MailRewardTooltipRoot = nullptr;
	}
	if (!RenderedMailRewardRows.IsValidIndex(ActiveMailRewardTooltipIndex)
		|| RenderedMailRewardRows[ActiveMailRewardTooltipIndex].Kind == ERokMailRewardKind::Currency
		|| !ModeCanvas)
	{
		return;
	}
	MailRewardTooltipRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("MailRewardTooltipRoot"));
	MailRewardTooltipRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ModeCanvas, MailRewardTooltipRoot, FVector2D::ZeroVector,
		CurrentExpandedDesignSize, 100);
	const FString& Name = RenderedMailRewardRows[ActiveMailRewardTooltipIndex].DisplayName;
	const float Width = 40.0f + Name.Len() * 18.0f;
	const FVector2D TipSize(Width, 60.0f);
	const float Left = FMath::Clamp(MailRewardTooltipAnchor.X - Width * 0.5f,
		10.0f, CurrentExpandedDesignSize.X - Width - 10.0f);
	const float Top = FMath::Max(90.0f, MailRewardTooltipAnchor.Y - TipSize.Y - 12.0f);
	const FVector2D Position(Left, Top);
	const float ArrowX = MailRewardTooltipAnchor.X - Left - 7.5f;
	if (ActiveModePage)
	{
		if (UClass* TooltipClass = ResolveFeaturePageClass(TEXT("MailRewardTooltip")))
		{
			if (auto* Page = CreateWidget<URokUiPageWidget>(this, TooltipClass))
			{
				Page->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				SetFeaturePageText(*Page, TEXT("MailRewardTooltipText"), FText::FromString(Name));
				for (const FName WidgetName : {FName(TEXT("MailRewardTooltipArrow")), FName(TEXT("MailRewardTooltipArrowHit"))})
				{
					if (auto* Widget = Page->Get<UWidget>(WidgetName))
					{
						if (auto* ArrowSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
						{
							ArrowSlot->SetPosition(FVector2D(ArrowX, ArrowSlot->GetPosition().Y));
						}
					}
				}
				BindPageAction(*Page, TEXT("MailRewardTooltipBodyHit"), -2698);
				BindPageAction(*Page, TEXT("MailRewardTooltipArrowHit"), -2698);
				PlaceOnCanvas(MailRewardTooltipRoot, Page, Position, TipSize);
				return;
			}
		}
	}
	PlaceOnCanvas(MailRewardTooltipRoot, MakeAtlasBorder(
		TEXT("MailRewardTooltipBackground"), CommonAtlasPath,
		SpriteRect(774, 276, 72, 72), CommonAtlasSize, TipSize,
		FMargin(21.0f / 72, 23.0f / 72, 21.0f / 72, 24.0f)),
		Position, TipSize, 1);
	UTextBlock* Text = MakeScreenText(TEXT("MailRewardTooltipText"), 14,
		FLinearColor(0.137f, 0.137f, 0.125f, 1.0f), false, ETextJustify::Center);
	Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18.0f * 72.0f / 96.0f));
	Text->SetShadowOffset(FVector2D::ZeroVector);
	Text->SetText(FText::FromString(Name));
	PlaceOnCanvas(MailRewardTooltipRoot, Text, Position + FVector2D(20.0f, 20.0f),
		FVector2D(Width - 40.0f, 20.0f), 2);
	PlaceOnCanvas(MailRewardTooltipRoot, MakeAtlasImage(
		TEXT("MailRewardTooltipArrow"), CommonAtlasPath,
		SpriteRect(120, 137, 15, 12), CommonAtlasSize, FVector2D(15.0f, 12.0f)),
		Position + FVector2D(MailRewardTooltipAnchor.X - Left - 7.5f, TipSize.Y - 1.4f),
		FVector2D(15.0f, 12.0f), 2);
	// Only the source body and arrow Graphic rects keep their own pointer hits.
	// The space beside the arrow remains available to underlying mail controls.
	PlaceOnCanvas(MailRewardTooltipRoot, MakeActionButton(
		TEXT("MailRewardTooltipBodyHit"), -2698, nullptr, FIntRect(), FIntPoint(1, 1),
		TipSize), Position, TipSize, 3);
	PlaceOnCanvas(MailRewardTooltipRoot, MakeActionButton(
		TEXT("MailRewardTooltipArrowHit"), -2698, nullptr, FIntRect(), FIntPoint(1, 1),
		FVector2D(15.0f, 12.0f)),
		Position + FVector2D(MailRewardTooltipAnchor.X - Left - 7.5f, TipSize.Y - 1.4f),
		FVector2D(15.0f, 12.0f), 4);
}

UCanvasPanel* URokUnityFeatureScreenWidget::MakeMailRow(int32 Index, const FRokMailRewardTemplate* MailTemplate)
{
	UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("MailRow_%d"), Index));
	const FRokMailLocalUnreadRow* LocalRow = bLocalMailUnreadFixtureState ? MailLocalUnreadState.Find(Index) : nullptr;
	const bool bSelected = Index == SelectedMailIndex;
	const FVector2D RowBackgroundSize = bSelected ? FVector2D(467.0f, 104.0f) : FVector2D(443.0f, 100.0f);
	UCanvasPanel* RowBackground = MakeAtlasBorder(*FString::Printf(TEXT("MailRowBackground_%d"), Index), CommonAtlasPath, bSelected ? SideSelectedRect : SideNormalRect, CommonAtlasSize, RowBackgroundSize, bSelected ? SideSelectedMargin : SideNormalMargin);
	if (bSelected)
	{
		RowBackground->SetRenderTransformPivot(FVector2D(0.5f));
		RowBackground->SetRenderScale(FVector2D(-1.0f, 1.0f));
	}
	PlaceOnCanvas(Row, RowBackground, bSelected ? FVector2D(-5.0f, 10.23f) : FVector2D(0.5f, 12.23f), RowBackgroundSize);
	PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("MailRowIcon_%d"), Index), MailAtlasPath, SpriteRect(1821, 559, 77, 75), MailAtlasSize, FVector2D(77.0f, 75.0f)), FVector2D(15.5f, 24.73f), FVector2D(77.0f, 75.0f), 2);
	UTextBlock* Title = MakeScreenText(*FString::Printf(TEXT("MailRowTitle_%d"), Index), 15,
		bSelected ? FLinearColor::Black : FLinearColor::FromSRGBColor(FColor(226, 209, 155)), true);
	Title->SetShadowOffset(FVector2D::ZeroVector);
	Title->SetText(LocalRow ? FText::FromString(LocalRow->Title) : (MailTemplate && Index == 0
		? FText::FromString(MailTemplate->Title)
		: NSLOCTEXT("RokFeature", "MailRowReward", "活动奖励补发")));
	PlaceOnCanvas(Row, Title, FVector2D(100.5f, 23.33f), FVector2D(313.0f, 31.25f), 3);
	UTextBlock* Subject = MakeScreenText(*FString::Printf(TEXT("MailRowSubject_%d"), Index), 14,
		bSelected ? FLinearColor::Black : FLinearColor::White);
	Subject->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18.0f * 72.0f / 96.0f));
	Subject->SetShadowOffset(FVector2D::ZeroVector);
	Subject->SetText(LocalRow ? FText::FromString(LocalRow->Summary) : (Index == 1
		? NSLOCTEXT("RokFeature", "MailSubjectBanquet", "英豪盛宴活动奖励补发")
		: (MailTemplate && Index == 0
			? FText::FromString(MailTemplate->Subtitle)
			: NSLOCTEXT("RokFeature", "MailSubjectConquest", "征服之始活动奖励补发"))));
	PlaceOnCanvas(Row, Subject, FVector2D(100.5f, 49.73f), FVector2D(313.0f, 32.13f), 3);
	UTextBlock* Date = MakeScreenText(*FString::Printf(TEXT("MailRowDate_%d"), Index), 14, bSelected ? FLinearColor(0.45f, 0.43f, 0.37f, 1.0f) : FLinearColor(0.55f, 0.55f, 0.54f, 1.0f));
	// The source mail template has no EmailInfo.sendTime; do not invent one.
	Date->SetText(FText::GetEmpty());
	PlaceOnCanvas(Row, Date, FVector2D(100.5f, 74.23f), FVector2D(313.0f, 24.94f), 3);
	if (LocalRow ? !LocalRow->bRead : Index > 0)
	{
		PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("MailUnread_%d"), Index), CommonAtlasPath, SpriteRect(1029, 489, 20, 20), CommonAtlasSize, FVector2D(18.0f)), FVector2D(2.0f, 1.0f), FVector2D(18.0f), 4);
	}
	if (!LocalRow && Index != 1)
	{
		auto* Pulse = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("MailAttachmentPulse_%d"), Index));
		Pulse->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		AttachMailGiftMesh(Index, *Pulse);
		PlaceOnCanvas(Row, Pulse, FVector2D(392.32f, 31.43f), FVector2D(28.0f, 25.0f), 4);
	}
	PlaceOnCanvas(Row, MakeActionButton(*FString::Printf(TEXT("MailRowHit_%d"), Index), 220 + Index, nullptr, FIntRect(), FIntPoint(1, 1), FVector2D(457.0f, 114.24f)), FVector2D::ZeroVector, FVector2D(457.0f, 114.24f), 10);
	return Row;
}

FSlateBrush URokUnityFeatureScreenWidget::MakeRewardQualityBrush(int32 Quality, FVector2D Size) const
{
	return MakeAtlasBrush(LoadUiTexture(Item0AtlasPath), ResolveBagQualityRect(Quality), Item0AtlasSize, Size);
}

const TArray<FRokTaskRewardPreviewRow>& URokUnityFeatureScreenWidget::GetConfiguredRewardPreview(int32 GroupId)
{
	const auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	int64 Country = 0;
	if (Net) RokSprotoJson::TryGetInt64(Net->GetRoleInfo(), TEXT("country"), Country);
	if (Country != TaskRewardCivilization)
	{
		TaskRewardCivilization = Country;
		ServerTaskRewardCache.Reset();
	}
	if (const auto* Cached = ServerTaskRewardCache.Find(GroupId)) return *Cached;
	auto& Result = ServerTaskRewardCache.Add(GroupId);
	if (GroupId <= 0 || !GetGameInstance()) return Result;
	auto* Config = GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>();
	if (!Config) return Result;
	if (TaskRewardRowsByGroup.IsEmpty())
	{
		if (const auto* Rows = Config->GetTableRows(TEXT("ItemPackageShow")))
		{
			for (const auto& Row : *Rows)
			{
				int32 Group = 0;
				if (Row && Row->TryGetNumberField(TEXT("group"), Group)) TaskRewardRowsByGroup.Add(Group, Row);
			}
		}
	}
	TArray<TSharedPtr<FJsonObject>> Rows;
	TaskRewardRowsByGroup.MultiFind(GroupId, Rows);
	auto* Economy = GetEconomyCatalogSubsystem();
	if (Economy && !Economy->IsCatalogLoaded()) Economy->LoadCatalog();
	for (const auto& Row : Rows)
	{
		FString Restriction;
		if (Row->TryGetStringField(TEXT("civilization_limit"), Restriction) && !Restriction.IsEmpty())
		{
			TArray<FString> Countries;
			Restriction.ParseIntoArray(Countries, TEXT("|"));
			if (!Countries.Contains(FString::Printf(TEXT("%lld"), Country))) continue;
		}
		FRokTaskRewardPreviewRow Reward;
		int32 Kind = 0, Id = 0;
		if (!Row->TryGetNumberField(TEXT("ID"), Reward.PackageRowId)
			|| !Row->TryGetNumberField(TEXT("type"), Kind)
			|| !Row->TryGetNumberField(TEXT("typeData"), Id)
			|| !Row->TryGetNumberField(TEXT("number"), Reward.Quantity) || Reward.Quantity <= 0) continue;
		const TCHAR* Atlas = nullptr;
		if (Kind == 100)
		{
			FRokMailRewardRow Currency;
			Currency.Id = Id;
			Reward.CurrencyId = Id;
			ResolveMailRewardIcon(Currency, Atlas, Reward.IconRect, Reward.IconAtlasSize);
			static const TCHAR* Names[] = {TEXT("粮食"), TEXT("木材"), TEXT("石料"), TEXT("金币"), TEXT("宝石")};
			if (Id >= 100 && Id < 100 + UE_ARRAY_COUNT(Names)) Reward.DisplayName = Names[Id - 100];
		}
		else if (Kind == 200)
		{
			FRokUnityEconomyItem Item;
			if (Economy && Economy->FindItemById(Id, Item))
			{
				Reward.DisplayName = Item.DisplayNameZhCn;
				Reward.Quality = Item.Quality;
				Reward.TopLabel = ResolveEconomyItemTopLabel(Item);
				ResolveEconomyItemIcon(Item, Atlas, Reward.IconRect, Reward.IconAtlasSize);
			}
		}
		if (Reward.DisplayName.IsEmpty()) Reward.DisplayName = FString::Printf(TEXT("奖励 %d"), Id);
		if (Atlas) Reward.AtlasPath = Atlas;
		Result.Add(MoveTemp(Reward));
	}
	Result.Sort([](const FRokTaskRewardPreviewRow& A, const FRokTaskRewardPreviewRow& B)
	{
		return A.PackageRowId < B.PackageRowId;
	});
	return Result;
}

void URokUnityFeatureScreenWidget::SubmitTaskClaim(ERokTaskClaim Kind, int32 Id)
{
	auto* Tasks = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerTaskSubsystem>() : nullptr;
	FString Error;
	const TWeakObjectPtr<URokUnityFeatureScreenWidget> WeakThis(this);
	if (!Tasks || !Tasks->Claim(Kind, Id, [WeakThis](const FString& Result)
	{
		auto* Widget = WeakThis.Get();
		if (!Widget || Widget->ActiveMode != ERokUnityFeatureScreenMode::Tasks) return;
		Widget->RefreshServerTaskPresentation();
		Widget->ShowBoundaryFeedback(Result.IsEmpty() ? NSLOCTEXT("RokFeature", "TaskClaimConfirmed", "已领取奖励")
			: FText::FromString(Result));
	}, Error))
	{
		ShowBoundaryFeedback(FText::FromString(Error.IsEmpty() ? TEXT("任务状态尚未同步。") : Error));
		return;
	}
	ShowBoundaryFeedback(NSLOCTEXT("RokFeature", "TaskClaimPending", "正在领取奖励…"));
}

void URokUnityFeatureScreenWidget::RefreshServerTaskPresentation()
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Tasks || bTaskTestFixtureState) return;
	const UGameInstance* Instance = GetGameInstance();
	const URokServerTaskSubsystem* Server = Instance ? Instance->GetSubsystem<URokServerTaskSubsystem>() : nullptr;
	const FRokServerTaskSnapshot Snapshot = Server ? Server->GetSnapshot() : FRokServerTaskSnapshot();
	if (Snapshot.bAvailable == bLastServerTaskAvailable && Snapshot.bLoaded == bLastServerTaskLoaded
		&& Snapshot.Signature == LastServerTaskSignature) return;
	float PreviousScrollOffset = 0.0f;
	const TCHAR* ScrollName = SelectedTaskPage == 0 ? TEXT("TaskChapterScroll")
		: SelectedTaskPage == 2 ? TEXT("TaskDailyCatalogScroll") : TEXT("TaskMainScroll");
	if (const UScrollBox* Scroll = Cast<UScrollBox>(GetWidgetFromName(ScrollName)))
	{
		PreviousScrollOffset = Scroll->GetScrollOffset();
	}
	RebuildModeContent();
	if (UScrollBox* Scroll = Cast<UScrollBox>(GetWidgetFromName(ScrollName)))
	{
		Scroll->SetScrollOffset(PreviousScrollOffset);
	}
}

void URokUnityFeatureScreenWidget::RefreshTaskMainProjection()
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Tasks || SelectedTaskPage != 1
		|| bLastServerTaskAvailable
		|| !VisibleTaskStates.IsValidIndex(0)
		|| VisibleTaskStates[0].Page != FRokTaskRowState::EPage::MainTask) return;
	const URokUnityRuntimeStateSubsystem* RuntimeState = GetRuntimeStateSubsystem();
	if (!RuntimeState || !RuntimeState->IsRuntimeStateLoaded()) return;
	FRokBuildingState HallState;
	const FRokBuildingState* HallStatePtr = nullptr;
	if (UWorld* World = GetWorld())
	{
		TArray<AActor*> HallActors;
		UGameplayStatics::GetAllActorsWithTag(World, TEXT("UnityInitId_5"), HallActors);
		if (HallActors.Num() == 1)
		{
			if (URokBuildingUpgradeSubsystem* Buildings = World->GetSubsystem<URokBuildingUpgradeSubsystem>())
			{
				HallState = Buildings->GetBuildingStateForActor(HallActors[0]);
				HallStatePtr = &HallState;
			}
		}
	}
	const FRokUnityRuntimeTaskLocalProjection Projection =
		RuntimeState->GetActiveTaskLocalProjection(HallStatePtr);
	const FRokUnityRuntimeTaskState& Task = Projection.Task;
	if (Task.TaskId <= 0 || Task.TaskId != VisibleTaskStates[0].TaskId) return;
	UTextBlock* Title = Cast<UTextBlock>(GetWidgetFromName(TEXT("TaskTitle_0")));
	UTextBlock* Description = Cast<UTextBlock>(GetWidgetFromName(TEXT("TaskDescription_0")));
	if (!Title || !Description) return;
	const FText DesiredTitle = Projection.bProjected
		? FText::Format(NSLOCTEXT("RokFeature", "TaskDetailLocalTitle", "{0}·本地"), FText::FromString(Task.Name))
		: FText::FromString(Task.Name);
	const FString ResolvedDescription = ResolveTaskDescription(
		Task.Desc, Task.Param1, Task.Param2, Task.Require);
	const bool bProgressKnown = Task.ProgressRule.Equals(TEXT("building_level"), ESearchCase::IgnoreCase)
		|| Task.ProgressRule.Equals(TEXT("building_count"), ESearchCase::IgnoreCase);
	const FText DesiredDescription = bProgressKnown
		? FText::Format(NSLOCTEXT("RokFeature", "TaskProgressFormat", "{0} ({1}/{2})"),
			FText::FromString(ResolvedDescription), FText::AsNumber(Task.Current),
			FText::AsNumber(FMath::Max(1, Task.Target)))
		: FText::FromString(ResolvedDescription);
	const FText DesiredTooltip = Projection.bProjected
		? (Projection.bConditionMet
			? NSLOCTEXT("RokFeature", "TaskDetailLocalConditionTooltip", "本地条件已满足，服务器任务状态待同步")
			: NSLOCTEXT("RokFeature", "TaskDetailLocalProgressTooltip", "当前显示本地城市条件进度；服务器任务状态待同步"))
		: FText::GetEmpty();
	if (!Title->GetText().EqualTo(DesiredTitle)) Title->SetText(DesiredTitle);
	if (!Description->GetText().EqualTo(DesiredDescription)) Description->SetText(DesiredDescription);
	if (!Description->GetToolTipText().EqualTo(DesiredTooltip)) Description->SetToolTipText(DesiredTooltip);
	bTaskMainLocalProjectionRendered = Projection.bProjected;
	bTaskMainLocalConditionMet = Projection.bProjected && Projection.bConditionMet;
	TaskMainDisplayedCurrent = Task.Current;
}

#include "RokFeatureTaskChapterServer.inl"

void URokUnityFeatureScreenWidget::BuildServerTaskChapter(const FRokServerTaskSnapshot& Snapshot)
{
	// UI_Win_Quest and UI_Item_ChapterQuest on the 1440x810 source canvas.
	const FLinearColor Warm = FLinearColor::FromSRGBColor(FColor(216, 208, 179));
	const FLinearColor RowColor = FLinearColor::FromSRGBColor(FColor(53, 51, 49));
	const FLinearColor BandColor = FLinearColor::FromSRGBColor(FColor(43, 43, 42));
	auto* Chapter = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TaskChapterRoot"));
	PlaceOnCanvas(ModeCanvas, Chapter, CapturePoint(508, 173), FVector2D(737, 632), 5);

	SelectedTaskChapterGroupId = Snapshot.ChapterId;
	bSelectedTaskChapterMetadataKnown = Snapshot.bLoaded && Snapshot.ChapterId > 0;
	if (!bSelectedTaskChapterMetadataKnown)
	{
		auto* Notice = MakeScreenText(TEXT("TaskChapterMetaNotice"), 17, Warm);
		Notice->SetText(Snapshot.bLoaded
			? NSLOCTEXT("RokFeature", "TaskServerChapterEmpty", "暂无当前章节")
			: NSLOCTEXT("RokFeature", "TaskServerChapterLoading", "等待服务器同步章节进度"));
		PlaceOnCanvas(Chapter, Notice, FVector2D(10, 4), FVector2D(700, 33), 1);
		return;
	}

	FRokTaskChapterPreviewRow Metadata;
	if (RokTaskChapterConfigPreviewData::FindChapterById(TaskChapterGroups, Snapshot.ChapterId, Metadata))
	{
		BuildServerTaskChapterGuide(Metadata);
	}

	auto* RewardTitle = MakeScreenText(TEXT("TaskChapterRewardTitle"), 17, Warm);
	RewardTitle->SetText(NSLOCTEXT("RokFeature", "TaskChapterRewardsHeading", "章节奖励"));
	PlaceOnCanvas(Chapter, RewardTitle, FVector2D(12, 2), FVector2D(250, 30), 1);
	PlaceOnCanvas(Chapter, MakeSolidBorder(TEXT("TaskChapterRewardBand"), RowColor),
		FVector2D(0, 33), FVector2D(737, 82), 0);

	FRokTaskChapterRewardGroup Aggregate;
	if (Snapshot.ChapterRewardGroupId > 0
		&& RokTaskChapterRewardPreviewData::FindForChapter(TaskChapterRewardGroups, Snapshot.ChapterId, Aggregate))
	{
		for (int32 Index = 0; Index < Aggregate.Rows.Num(); ++Index)
		{
			const auto& Reward = Aggregate.Rows[Index];
			const FString Prefix = FString::Printf(TEXT("TaskChapterAggregateReward_%d_%d"), Snapshot.ChapterId, Index);
			const FVector2D CellPosition(29 + Index * 69, 46);
			int32 Quality = 2; // UI_Model_Item uses quality 2 for currency and troops.
			if (Reward.RewardType == 200)
			{
				auto* Config = GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>();
				if (Config) Config->GetIntFieldByKey(TEXT("Item"), TEXT("ID"), Reward.TypeData, TEXT("quality"), Quality);
			}
			PlaceOnCanvas(Chapter, MakeAtlasImage(*(Prefix + TEXT("Quality")), Item0AtlasPath,
				ResolveBagQualityRect(Quality), Item0AtlasSize, FVector2D(55)), CellPosition, FVector2D(55), 1);
			auto* Icon = MakeAtlasImage(*(Prefix + TEXT("Icon")), *Reward.AtlasObjectPath,
				Reward.IconRect, Reward.IconAtlasSize, FVector2D(50));
			Icon->SetToolTipText(FText::FromString(Reward.DisplayNameZhCn));
			Icon->SetVisibility(ESlateVisibility::Visible);
			PlaceOnCanvas(Chapter, Icon, CellPosition + FVector2D(2.5), FVector2D(50), 2);
			auto* Quantity = MakeScreenText(*(Prefix + TEXT("Quantity")), 8, Warm, true, ETextJustify::Right);
			Quantity->SetText(FText::AsNumber(Reward.Quantity));
			PlaceOnCanvas(Chapter, Quantity, CellPosition + FVector2D(1, 40), FVector2D(50, 16), 3);
			RenderedTaskChapterAggregateRewardPackageIds.Add(Reward.PackageRowId);
		}
	}

	auto* Progress = MakeScreenText(TEXT("TaskChapterMetaNotice"), 16, Warm, true, ETextJustify::Center);
	Progress->SetText(Snapshot.bChapterProgressKnown
		? FText::Format(NSLOCTEXT("RokFeature", "TaskChapterProgress", "进度:{0}/{1}"),
			FText::AsNumber(Snapshot.ChapterCurrent), FText::AsNumber(Snapshot.ChapterTarget))
		: NSLOCTEXT("RokFeature", "TaskChapterPendingProgress", "进度待同步"));
	PlaceOnCanvas(Chapter, Progress, FVector2D(Snapshot.bChapterClaimable ? 420 : 548, 59), FVector2D(150, 28), 2);
	if (Snapshot.bChapterClaimable)
	{
		const FVector2D ButtonSize(156, 49);
		PlaceOnCanvas(Chapter, MakeActionButton(TEXT("TaskChapterClaimAction"), 6100,
			CommonAtlasPath, MiniGreenButtonRect, CommonAtlasSize, ButtonSize,
			NSLOCTEXT("RokFeature", "TaskClaimReward", "领取奖励"), 17, FLinearColor::White,
			ESlateBrushDrawType::Box, MiniGreenMargin), FVector2D(572, 49), ButtonSize, 3);
	}

	auto* Goals = MakeScreenText(TEXT("TaskChapterGoalsHeading"), 17, Warm);
	Goals->SetText(NSLOCTEXT("RokFeature", "TaskChapterGoals", "目标"));
	PlaceOnCanvas(Chapter, Goals, FVector2D(12, 126), FVector2D(250, 30), 1);

	auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TaskChapterScroll"));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	Scroll->SetAllowOverscroll(false);
	Scroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(Chapter, Scroll, FVector2D(0, 161), FVector2D(737, 468.5), 3);
	for (const FRokServerTaskView& Task : Snapshot.ChapterTasks)
	{
		RenderedTaskChapterTaskIds.Add(Task.TaskId);
		const FString Prefix = FString::Printf(TEXT("TaskChapterTask_%d"), Task.TaskId);
		auto* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *(Prefix + TEXT("Row")));
		PlaceOnCanvas(Row, MakeSolidBorder(*(Prefix + TEXT("Background")), RowColor),
			FVector2D::ZeroVector, FVector2D(737, 98), 0);
		PlaceOnCanvas(Row, MakeSolidBorder(*(Prefix + TEXT("RewardBand")), BandColor),
			FVector2D(4, 41), FVector2D(519, 43), 1);
		auto* Detail = MakeScreenText(*FString::Printf(TEXT("TaskChapterTaskDescription_%d"), Task.TaskId),
			17, FLinearColor::White);
		Detail->SetText(Task.bProgressKnown
			? FText::Format(NSLOCTEXT("RokFeature", "TaskChapterObjectiveProgress", "{0} ({1}/{2})"),
				FText::FromString(Task.Description), FText::AsNumber(Task.Current), FText::AsNumber(Task.Target))
			: FText::FromString(Task.Description));
		Detail->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Detail->SetToolTipText(Detail->GetText());
		PlaceOnCanvas(Row, Detail, FVector2D(16, 12), FVector2D(706, 30), 2);

		auto* RewardLabel = MakeScreenText(*(Prefix + TEXT("RewardLabel")), 14, Warm);
		RewardLabel->SetText(NSLOCTEXT("RokFeature", "TaskRewardsLabel", "奖励"));
		PlaceOnCanvas(Row, RewardLabel, FVector2D(16, 51), FVector2D(72, 27), 2);
		FRokTaskChapterRewardGroup TaskRewards;
		if (Task.RewardGroupId > 0 && RokTaskChapterRewardPreviewData::FindForTask(TaskChapterRewardGroups, Task.TaskId, TaskRewards))
		{
			for (int32 Index = 0; Index < TaskRewards.Rows.Num(); ++Index)
			{
				const auto& Reward = TaskRewards.Rows[Index];
				const FString RewardPrefix = FString::Printf(TEXT("TaskChapterTaskReward_%d_%d"), Task.TaskId, Index);
				const float X = 95 + Index * 103;
				PlaceOnCanvas(Row, MakeAtlasImage(*(RewardPrefix + TEXT("Icon")), *Reward.AtlasObjectPath,
					Reward.IconRect, Reward.IconAtlasSize, FVector2D(29)), FVector2D(X, 48), FVector2D(29), 2);
				auto* Quantity = MakeScreenText(*(RewardPrefix + TEXT("Quantity")), 12, Warm);
				Quantity->SetText(FText::AsNumber(Reward.Quantity));
				PlaceOnCanvas(Row, Quantity, FVector2D(X + 32, 53), FVector2D(71, 23), 3);
				RenderedTaskChapterTaskRewardPackageIds.Add(Reward.PackageRowId);
			}
		}

		const bool bClaim = Task.State == ERokServerTaskState::Finished;
		if (bClaim || Task.State == ERokServerTaskState::Unfinished)
		{
			FRokTaskRowState Action;
			Action.TaskId = Task.TaskId;
			Action.Page = FRokTaskRowState::EPage::ChapterTask;
			Action.SourceAuthority = TEXT("server");
			Action.Type = Task.Type;
			Action.Param1 = Task.Param1;
			Action.Param2 = Task.Param2;
			Action.Require = Task.Require;
			Action.State = bClaim ? FRokTaskRowState::EState::Finished : FRokTaskRowState::EState::Unfinished;
			Action.Intent = bClaim ? FRokTaskRowState::EIntent::Claim : FRokTaskRowState::EIntent::Go;
			const int32 RowIndex = VisibleTaskStates.Add(Action);
			const FVector2D ButtonSize(208, 50);
			PlaceOnCanvas(Row, MakeActionButton(*(Prefix + (bClaim ? TEXT("Claim") : TEXT("Go"))), 6000 + RowIndex,
				CommonAtlasPath, bClaim ? MiniGreenButtonRect : MiniBlueButtonRect, CommonAtlasSize, ButtonSize,
				bClaim ? NSLOCTEXT("RokFeature", "TaskClaimReward", "领取奖励") : NSLOCTEXT("RokFeature", "TaskGo", "前往"),
				17, FLinearColor::White, ESlateBrushDrawType::Box, bClaim ? MiniGreenMargin : MiniBlueMargin),
				FVector2D(521, 29), ButtonSize, 3);
		}
		else
		{
			auto* State = MakeScreenText(*(Prefix + TEXT("State")), 14, Warm, true, ETextJustify::Center);
			State->SetText(Task.State == ERokServerTaskState::Received
				? NSLOCTEXT("RokFeature", "TaskChapterCompleted", "已完成")
				: NSLOCTEXT("RokFeature", "TaskChapterUnknownState", "状态待同步"));
			PlaceOnCanvas(Row, State, FVector2D(521, 48), FVector2D(208, 28), 3);
		}
		Scroll->AddChild(WrapFixedSize(Row, *(Prefix + TEXT("Size")), FVector2D(737, 104)));
	}
	if (Snapshot.ChapterTasks.IsEmpty())
	{
		auto* Empty = MakeScreenText(TEXT("TaskChapterServerEmpty"), 17, Warm);
		Empty->SetText(NSLOCTEXT("RokFeature", "TaskChapterServerNoTasks", "当前章节暂无任务"));
		Scroll->AddChild(WrapFixedSize(Empty, TEXT("TaskChapterServerEmptySize"), FVector2D(737, 90)));
	}
}

void URokUnityFeatureScreenWidget::BuildServerTaskChapterGuide(const FRokTaskChapterPreviewRow& Metadata)
{
	const TCHAR* QuestAtlas = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_quest.rok_fullscreen_quest");
	const FIntPoint QuestAtlasSize(512, 128);
	const FVector2D DialogPosition = CapturePoint(192, 174);
	const FVector2D DialogSize(302, 156);
	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("TaskChapterDialogPanel"), CommonAtlasPath,
		SpriteRect(1952, 906, 74, 67), CommonAtlasSize, FVector2D(286, 149),
		FMargin(23.f / 74, 13.f / 67, 21.f / 74, 22.f / 67)), DialogPosition + FVector2D(8, 3), FVector2D(286, 149), 3);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FVector2D LineSize(302, 17);
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(Index == 0 ? TEXT("TaskChapterDialogTop") : TEXT("TaskChapterDialogBottom"),
			QuestAtlas, SpriteRect(1, 110, 302, 17), QuestAtlasSize, LineSize),
			DialogPosition + FVector2D(0, Index * (DialogSize.Y - LineSize.Y)), LineSize, 4);
	}
	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("TaskChapterDialogArrow"), QuestAtlas,
		SpriteRect(130, 78, 13, 11), QuestAtlasSize, FVector2D(13, 11)),
		DialogPosition + FVector2D(262, 129), FVector2D(13, 11), 4);

	auto* Dialogue = WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass(), TEXT("TaskChapterDialogue"));
	Dialogue->SetWrapTextAt(230);
	Dialogue->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(ModeCanvas, Dialogue, DialogPosition + FVector2D(26, 19), FVector2D(247, 114), 5);
	PopulateServerTaskChapterGuide(Metadata, *Dialogue, nullptr);
}

void URokUnityFeatureScreenWidget::PopulateServerTaskChapterGuide(const FRokTaskChapterPreviewRow& Metadata,
	URichTextBlock& Dialogue, UCanvasPanel* ModelSlot)
{
	FTextBlockStyle TextStyle;
	const bool bAuthoredStyle = ModelSlot && TryGetFeatureTextStyle(TEXT("TaskChapterDialogueDefault"), TextStyle);
	if (!bAuthoredStyle)
	{
		TextStyle.SetFont(URokMainHudWidget::MakeUnitySourceFont(17, true))
			.SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	auto* Styles = NewObject<UDataTable>(&Dialogue);
	Styles->RowStruct = FRichTextStyleRow::StaticStruct();
	FRichTextStyleRow Default;
	Default.TextStyle = TextStyle;
	Styles->AddRow(TEXT("Default"), Default);
	FRichTextStyleRow Emphasis = Default;
	if (!bAuthoredStyle || !TryGetFeatureTextStyle(TEXT("TaskChapterDialogueHighlight"), Emphasis.TextStyle))
	{
		Emphasis.TextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.43f, 0.02f)));
	}
	Styles->AddRow(TEXT("Emphasis"), Emphasis);
	Dialogue.SetTextStyleSet(Styles);
	if (bAuthoredStyle)
	{
		Dialogue.SetDefaultTextStyle(TextStyle);
	}
	FString Story = Metadata.Description2.ZhCn;
	Story.ReplaceInline(TEXT("<b>"), TEXT(""));
	Story.ReplaceInline(TEXT("</b>"), TEXT(""));
	Story.ReplaceInline(TEXT("<color=orange>"), TEXT("<Emphasis>"));
	Story.ReplaceInline(TEXT("</color>"), TEXT("</>"));
	Dialogue.SetText(FText::FromString(Story));

	const auto* Instance = GetGameInstance();
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	int64 HallLevel = 0;
	int32 Age = 1;
	if (Net && Config && RokSprotoJson::TryGetInt64(Net->GetRoleInfo(), TEXT("level"), HallLevel))
	{
		Config->GetIntFieldByKey(TEXT("BuildingTownCenter"), TEXT("ID"), static_cast<int32>(HallLevel), TEXT("age"), Age);
	}
	if (Metadata.AgeCharacterAssets.IsValidIndex(Age - 1) && !Metadata.AgeCharacterAssets[Age - 1].IsEmpty())
	{
		auto* Guide = WidgetTree->ConstructWidget<URokCommanderMeshWidget>(URokCommanderMeshWidget::StaticClass(), TEXT("TaskChapterGuide"));
		if (Guide->LoadQuestGuide(Metadata.AgeCharacterAssets[Age - 1]))
		{
			Guide->SetClipping(EWidgetClipping::ClipToBoundsAlways);
			Guide->SetVisibility(ESlateVisibility::HitTestInvisible);
#if !UE_BUILD_SHIPPING
			if (FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
				&& FParse::Param(FCommandLine::Get(), TEXT("RokNoNet"))
				&& FParse::Param(FCommandLine::Get(), TEXT("RokTaskChapterStaticCapture")))
			{
				Guide->FreezeAnimationForCapture(0.75f);
			}
#endif
			if (ModelSlot)
			{
				auto* ModelCanvasSlot = ModelSlot->AddChildToCanvas(Guide);
				ModelCanvasSlot->SetAnchors(FAnchors(0, 0, 1, 1));
				ModelCanvasSlot->SetOffsets(FMargin(0));
			}
			else
			{
				PlaceOnCanvas(ModeCanvas, Guide, CapturePoint(124, 350), FVector2D(365, 458), 3);
			}
		}
	}
}

void URokUnityFeatureScreenWidget::BuildTaskChapter(const URokUiPageWidget* Page, UClass* RowClass,
	UClass* AggregateRewardClass, UClass* TaskRewardClass)
{
	// The group chooser is a local config browser. Unity's live current chapter
	// depends on account state that is unavailable in this read-only screen.
	UCanvasPanel* ChapterRoot = Page ? Page->Get<UCanvasPanel>(TEXT("TaskChapterRoot")) : nullptr;
	if (!ChapterRoot)
	{
		ChapterRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TaskChapterRoot"));
		PlaceOnCanvas(ModeCanvas, ChapterRoot, CapturePoint(176.0f, 136.0f), FVector2D(1088.0f, 664.0f), 5);
	}
	RenderedTaskChapterTaskIds.Reset();
	RenderedTaskChapterTaskRewardPackageIds.Reset();
	RenderedTaskChapterAggregateRewardPackageIds.Reset();

	for (int32 GroupId = 1; GroupId <= 7; ++GroupId)
	{
		const bool bSelected = GroupId == SelectedTaskChapterGroupId;
		const FString ButtonName = FString::Printf(TEXT("TaskChapterGroupButton_%d"), GroupId);
		const FLinearColor LabelColor = bSelected ? FLinearColor::White : FLinearColor(0.78f, 0.74f, 0.66f, 1.0f);
		if (Page)
		{
			BindPageAction(*Page, *ButtonName, 339 + GroupId);
			if (auto* Label = Page->Get<UTextBlock>(*(ButtonName + TEXT("Label"))))
			{
				FSlateColor AuthoredColor(LabelColor);
				TryGetFeatureTextColor(bSelected ? TEXT("TaskChapterCatalogGroupSelected")
					: TEXT("TaskChapterCatalogGroupUnselected"), AuthoredColor);
				Label->SetColorAndOpacity(AuthoredColor);
			}
			continue;
		}
		const FVector2D Position(28.0f + (GroupId - 1) * 105.0f, 31.0f);
		PlaceOnCanvas(ChapterRoot,
			MakeActionButton(*ButtonName, 339 + GroupId, CommonAtlasPath, CommonPanelRect, CommonAtlasSize,
				FVector2D(99.0f, 43.0f), FText::Format(NSLOCTEXT("RokFeature", "TaskChapterGroupButton", "第{0}组"), FText::AsNumber(GroupId)),
				16, LabelColor,
				ESlateBrushDrawType::Box, CommonPanelMargin),
			Position, FVector2D(99.0f, 43.0f), 2);
	}

	FRokTaskChapterPreviewRow Chapter;
	const bool bChapterFound = RokTaskChapterConfigPreviewData::FindChapterById(
		TaskChapterGroups, SelectedTaskChapterGroupId, Chapter);
	bSelectedTaskChapterMetadataKnown = bChapterFound && Chapter.bMetadataKnown;
	const FText GroupTitle = bChapterFound ? FText::FromString(Chapter.DisplayTitle())
		: NSLOCTEXT("RokFeature", "TaskChapterUnavailable", "章节配置不可用");
	const FText Notice = !TaskChapterLoadError.IsEmpty()
		? FText::FromString(TEXT("章节配置暂不可浏览。"))
		: !TaskChapterRewardLoadError.IsEmpty()
			? NSLOCTEXT("RokFeature", "TaskChapterRewardUnavailable", "奖励配置暂不可浏览；章节任务配置仍可查看。")
		: !bSelectedTaskChapterMetadataKnown
			? NSLOCTEXT("RokFeature", "TaskChapterMissingMetadata", "本组章节标题资料缺失；仅展示任务配置，账号进度未同步。")
			: NSLOCTEXT("RokFeature", "TaskChapterConfigOnly", "本地配置浏览 · 账号章节进度与领取状态未同步");
	if (Page)
	{
		SetFeaturePageText(*Page, TEXT("TaskChapterGroupTitle"), GroupTitle);
		SetFeaturePageText(*Page, TEXT("TaskChapterMetaNotice"), Notice);
	}
	else
	{
		auto* GroupTitleText = MakeScreenText(TEXT("TaskChapterGroupTitle"), 23,
			FLinearColor::FromSRGBColor(FColor(230, 217, 180)), true);
		GroupTitleText->SetText(GroupTitle);
		PlaceOnCanvas(ChapterRoot, GroupTitleText, FVector2D(27.0f, 105.0f), FVector2D(720.0f, 39.0f), 2);
		auto* NoticeText = MakeScreenText(TEXT("TaskChapterMetaNotice"), 15, FLinearColor(0.78f, 0.75f, 0.67f, 1.0f));
		NoticeText->SetText(Notice);
		PlaceOnCanvas(ChapterRoot, NoticeText, FVector2D(27.0f, 151.0f), FVector2D(1010.0f, 29.0f), 2);
	}
	// Unity's ResourcesConsumeReward and UI_Model_Item both format positive
	// quantities as N0; use stable comma grouping independent of UE locale.
	const auto RewardQuantityText = [](int64 Quantity)
	{
		const FString Digits = LexToString(Quantity);
		FString Grouped;
		for (int32 Index = 0; Index < Digits.Len(); ++Index)
		{
			if (Index > 0 && (Digits.Len() - Index) % 3 == 0)
			{
				Grouped.AppendChar(TEXT(','));
			}
			Grouped.AppendChar(Digits[Index]);
		}
		return FText::FromString(Grouped);
	};

	const auto PopulateReward = [this, &RewardQuantityText](URokUiPageWidget& RewardPage, const TCHAR* Field,
		int32 OwnerId, int32 Index, const FRokTaskChapterRewardPreviewRow& Reward)
	{
		RewardPage.LegacyPrefix = TEXT("TaskChapter");
		RewardPage.LegacySuffix = FString::Printf(TEXT("_%d_%d"), OwnerId, Index);
		const FString Base(Field);
		SetFeaturePageText(RewardPage, *(Base + TEXT("Quantity")), RewardQuantityText(Reward.Quantity));
		if (auto* Icon = RewardPage.Get<UImage>(*(Base + TEXT("Icon"))))
		{
			UTexture2D* Texture = LoadUiTexture(*Reward.AtlasObjectPath);
			Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (Texture)
			{
				Icon->SetBrush(MakeAtlasBrush(Texture, Reward.IconRect, Reward.IconAtlasSize, Icon->GetBrush().GetImageSize()));
			}
		}
	};

	// The chapter-title reward list is separate from the per-task rewards.
	// Chapter 7 has no source aggregate metadata, so it gets no synthetic cells.
	FRokTaskChapterRewardGroup Aggregate;
	const bool bAggregateFound = RokTaskChapterRewardPreviewData::FindForChapter(
		TaskChapterRewardGroups, SelectedTaskChapterGroupId, Aggregate);
	const bool bShowMissingAggregate = !bAggregateFound && bChapterFound && !bSelectedTaskChapterMetadataKnown;
	if (Page)
	{
		if (auto* Missing = Page->Get<UTextBlock>(TEXT("TaskChapterAggregateRewardUnavailable")))
		{
			Missing->SetVisibility(bShowMissingAggregate ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	if (bAggregateFound)
	{
		const int32 Count = Aggregate.Rows.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FRokTaskChapterRewardPreviewRow& Reward = Aggregate.Rows[Index];
			RenderedTaskChapterAggregateRewardPackageIds.Add(Reward.PackageRowId);
			// UI_Item_Bag/UI_Model_Item is 134 square under items scale .42.
			// ListView spacing 30 at the same scale gives a 68.88-pixel pitch.
			const FVector2D CellSize(56.28f, 56.28f);
			constexpr float CellPitch = 68.88f;
			const float RightOffset = (Count - 1 - Index) * CellPitch + CellSize.X;
			if (Page)
			{
				auto* Host = Page->Get<UCanvasPanel>(TEXT("TaskChapterAggregateRewardHost"));
				if (auto* CellPage = CreateWidget<URokUiPageWidget>(this, AggregateRewardClass))
				{
					PopulateReward(*CellPage, TEXT("AggregateReward"), SelectedTaskChapterGroupId, Index, Reward);
					CellPage->SetVisibility(ESlateVisibility::HitTestInvisible);
					PlaceOnCanvas(Host, CellPage, FVector2D(CastChecked<UCanvasPanelSlot>(Host->Slot)->GetSize().X - RightOffset, 0),
						CellSize);
					continue;
				}
			}
			const FString Prefix = FString::Printf(TEXT("TaskChapterAggregateReward_%d_%d"),
				SelectedTaskChapterGroupId, Index);
			UCanvasPanel* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *Prefix);
			Cell->SetVisibility(ESlateVisibility::HitTestInvisible);
			PlaceOnCanvas(ChapterRoot, Cell, FVector2D(1040.0f - RightOffset, 96.0f), CellSize, 3);
			PlaceOnCanvas(Cell, MakeAtlasImage(*(Prefix + TEXT("Icon")),
				*Reward.AtlasObjectPath, Reward.IconRect, Reward.IconAtlasSize, FVector2D(50.4f, 50.4f)),
				FVector2D(2.94f, 2.94f), FVector2D(50.4f, 50.4f), 1);
			// Source lbl_count (114.6 x 30, font 26) overlays the lower card
			// edge; after scale .42 its region is 48.13 x 12.6. Slate uses
			// point size 8 to render approximately the source 10.92 pixels.
			UScaleBox* QuantityFit = WidgetTree->ConstructWidget<UScaleBox>(
				UScaleBox::StaticClass(), *(Prefix + TEXT("QuantityFit")));
			QuantityFit->SetStretch(EStretch::ScaleToFit);
			QuantityFit->SetStretchDirection(EStretchDirection::DownOnly);
			QuantityFit->SetVisibility(ESlateVisibility::HitTestInvisible);
			UTextBlock* Quantity = MakeScreenText(*(Prefix + TEXT("Quantity")), 8,
				FLinearColor::FromSRGBColor(FColor(217, 212, 186)), true, ETextJustify::Right);
			Quantity->SetText(RewardQuantityText(Reward.Quantity));
			QuantityFit->SetContent(Quantity);
			if (UScaleBoxSlot* QuantitySlot = Cast<UScaleBoxSlot>(Quantity->Slot))
			{
				QuantitySlot->SetHorizontalAlignment(HAlign_Right);
				QuantitySlot->SetVerticalAlignment(VAlign_Top);
			}
			PlaceOnCanvas(Cell, QuantityFit, FVector2D(2.9f, 41.2f), FVector2D(48.13f, 12.6f), 2);
		}
	}
	else if (!Page && bShowMissingAggregate)
	{
		UTextBlock* Missing = MakeScreenText(TEXT("TaskChapterAggregateRewardUnavailable"), 14,
			FLinearColor::FromSRGBColor(FColor(164, 157, 146)));
		Missing->SetText(NSLOCTEXT("RokFeature", "TaskChapterAggregateRewardMissing", "本章奖励资料缺失"));
		PlaceOnCanvas(ChapterRoot, Missing, FVector2D(779.0f, 105.0f), FVector2D(263.0f, 35.0f), 3);
	}

	UScrollBox* ChapterScroll = Page ? Page->Get<UScrollBox>(TEXT("TaskChapterScroll")) : nullptr;
	if (!ChapterScroll)
	{
		ChapterScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TaskChapterScroll"));
		ChapterScroll->SetOrientation(Orient_Vertical);
		ChapterScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		ChapterScroll->SetAllowOverscroll(false);
		ChapterScroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
		PlaceOnCanvas(ChapterRoot, ChapterScroll, FVector2D(19.23f, 196.2f), FVector2D(737.0f, 468.5f), 3);
	}
	if (!bChapterFound)
	{
		return;
	}

	FRokResearchCatalog ResearchCatalog;
	bool bResearchCatalogLoadAttempted = false;
	bool bResearchCatalogLoaded = false;
	for (const int32 TaskId : Chapter.TaskIds)
	{
		FRokTaskChapterTaskPreviewRow Task;
		if (!RokTaskChapterConfigPreviewData::FindTaskById(TaskChapterTasks, TaskId, Task))
		{
			continue;
		}
		RenderedTaskChapterTaskIds.Add(TaskId);
		const FText Title = FText::FromString(Task.Title.ZhCn);
		// TaskMain p1/p2/p3 are source configuration parameters, not live progress.
		FString Description = Task.Description.ZhCn;
		Description.ReplaceInline(TEXT("{p1}"), *FString::FromInt(Task.Param1));
		Description.ReplaceInline(TEXT("{p2}"), *FString::FromInt(Task.Param2));
		Description.ReplaceInline(TEXT("{p3}"), *FString::FromInt(Task.Require));
		if (Description.Contains(TEXT("{sn1}")))
		{
			if (!bResearchCatalogLoadAttempted)
			{
				bResearchCatalogLoadAttempted = true;
				FString ResearchError;
				bResearchCatalogLoaded = ResearchCatalog.Load(ResearchError);
			}
			FRokResearchRow Research;
			Description.ReplaceInline(TEXT("{sn1}"),
				bResearchCatalogLoaded && ResearchCatalog.GetByStudyLevel(Task.Param1, Task.Param2, Research)
					? *Research.Name : TEXT("指定科技"));
		}
		const FText DescriptionValue = FText::FromString(Description);
		FRokTaskChapterRewardGroup RewardGroup;
		const bool bHasTaskRewards = RokTaskChapterRewardPreviewData::FindForTask(TaskChapterRewardGroups, TaskId, RewardGroup);
		if (Page)
		{
			if (auto* RowPage = CreateWidget<URokUiPageWidget>(this, RowClass))
			{
				auto* RewardHost = RowPage->Get<UCanvasPanel>(TEXT("TaskRewardHost"));
				const auto* RewardHostSlot = RewardHost ? Cast<UCanvasPanelSlot>(RewardHost->Slot) : nullptr;
				if (RewardHostSlot)
				{
					RowPage->LegacyPrefix = TEXT("TaskChapter");
					RowPage->LegacySuffix = FString::Printf(TEXT("_%d"), TaskId);
					RowPage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					SetFeaturePageText(*RowPage, TEXT("TaskTitle"), Title);
					SetFeaturePageText(*RowPage, TEXT("TaskDescription"), DescriptionValue);
					const double RewardRight = RewardHostSlot->GetSize().X;
					for (int32 Index = 0; bHasTaskRewards && Index < RewardGroup.Rows.Num(); ++Index)
					{
						const auto& Reward = RewardGroup.Rows[Index];
						RenderedTaskChapterTaskRewardPackageIds.Add(Reward.PackageRowId);
						if (auto* CellPage = CreateWidget<URokUiPageWidget>(this, TaskRewardClass))
						{
							PopulateReward(*CellPage, TEXT("TaskReward"), TaskId, Index, Reward);
							CellPage->SetVisibility(ESlateVisibility::HitTestInvisible);
							const FVector2D CellSize(104.0f, 40.0f);
							PlaceOnCanvas(RewardHost, CellPage, FVector2D(RewardRight - (Index + 1) * CellSize.X, 0), CellSize);
						}
					}
					CastChecked<UScrollBoxSlot>(ChapterScroll->AddChild(RowPage))->SetHorizontalAlignment(HAlign_Left);
					continue;
				}
			}
		}

		UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("TaskChapterTaskRow_%d"), TaskId));
		PlaceOnCanvas(Row, MakeAtlasBorder(*FString::Printf(TEXT("TaskChapterTaskBackground_%d"), TaskId),
			CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(740.0f, 100.0f), CommonPanelMargin),
			FVector2D::ZeroVector, FVector2D(740.0f, 100.0f), 0);
		UTextBlock* TitleText = MakeScreenText(*FString::Printf(TEXT("TaskChapterTaskTitle_%d"), TaskId),
			17, FLinearColor::FromSRGBColor(FColor(230, 217, 180)), true);
		TitleText->SetText(Title);
		TitleText->SetAutoWrapText(true);
		// Configuration-only title occupies the source account-action column.
		PlaceOnCanvas(Row, TitleText, FVector2D(14.0f, 16.0f), FVector2D(180.0f, 69.0f), 1);
		UTextBlock* DescriptionText = MakeScreenText(*FString::Printf(TEXT("TaskChapterTaskDescription_%d"), TaskId),
			17, FLinearColor::White, false, ETextJustify::Right);
		DescriptionText->SetText(DescriptionValue);
		DescriptionText->SetAutoWrapText(true);
		// Unity lbl_itemName presents TaskChapter.desc with live progress. This
		// preview keeps only the source description and leaves progress unknown.
		PlaceOnCanvas(Row, DescriptionText, FVector2D(200.7f, 8.4f), FVector2D(520.0f, 37.9f), 1);

		if (bHasTaskRewards)
		{
			// Source GridLayout: 130 x 50 cells, parent scale .8, right-anchored.
			for (int32 Index = 0; Index < RewardGroup.Rows.Num(); ++Index)
			{
				const FRokTaskChapterRewardPreviewRow& Reward = RewardGroup.Rows[Index];
				const FString Prefix = FString::Printf(TEXT("TaskChapterTaskReward_%d_%d"), TaskId, Index);
				UCanvasPanel* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *Prefix);
				Cell->SetVisibility(ESlateVisibility::HitTestInvisible);
				const FVector2D CellSize(104.0f, 40.0f);
				PlaceOnCanvas(Row, Cell,
					FVector2D(733.0f - (Index + 1) * CellSize.X, 44.0f), CellSize, 2);
				PlaceOnCanvas(Cell, MakeAtlasImage(*(Prefix + TEXT("Icon")),
					*Reward.AtlasObjectPath, Reward.IconRect, Reward.IconAtlasSize, FVector2D(33.6f, 33.6f)),
					FVector2D(0.0f, 3.2f), FVector2D(33.6f, 33.6f), 1);
				UTextBlock* Quantity = MakeScreenText(*(Prefix + TEXT("Quantity")), 11,
					FLinearColor::FromSRGBColor(FColor(235, 231, 211)), true);
				Quantity->SetText(RewardQuantityText(Reward.Quantity));
				PlaceOnCanvas(Cell, Quantity, FVector2D(35.0f, 5.0f), FVector2D(69.0f, 30.0f), 2);
				RenderedTaskChapterTaskRewardPackageIds.Add(Reward.PackageRowId);
			}
		}
		// SScrollPanel clamps its direct child to the 737-pixel viewport even at
		// HAlign_Left. Keep the source 740-pixel row in an explicit canvas slot;
		// only its 737-pixel wrapper is clipped by the source ScrollRect.
		UCanvasPanel* ViewportRow = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("TaskChapterTaskViewportRow_%d"), TaskId));
		PlaceOnCanvas(ViewportRow, Row, FVector2D::ZeroVector, FVector2D(740.0f, 104.0f));
		UScrollBoxSlot* RowSlot = CastChecked<UScrollBoxSlot>(ChapterScroll->AddChild(WrapFixedSize(ViewportRow,
			*FString::Printf(TEXT("TaskChapterTaskRowSize_%d"), TaskId), FVector2D(737.0f, 104.0f))));
		RowSlot->SetHorizontalAlignment(HAlign_Left);
	}
}

bool URokUnityFeatureScreenWidget::BeginTaskCompletedRowAnimation(UCanvasPanel* Row, TFunction<void()> OnComplete)
{
	if (TaskCompletedAnimationElapsed >= 0.0f || !IsValid(Row) || !Row->GetParent() || !OnComplete)
	{
		return false;
	}
	TaskCompletedAnimatedRow = Row;
	TaskCompletedAnimationCallback = MoveTemp(OnComplete);
	TaskCompletedAnimationElapsed = 0.0f;
	TaskCompletedAnimationStartedAtSeconds = FPlatformTime::Seconds();
	Row->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Row->SetRenderScale(FVector2D(1.0f));
	TArray<UWidget*> Descendants;
	Descendants.Add(Row);
	for (int32 DescendantIndex = 0; DescendantIndex < Descendants.Num(); ++DescendantIndex)
	{
		UWidget* Widget = Descendants[DescendantIndex];
		// Fade the outermost renderable widget once. UMG parent opacity also
		// affects descendants, so fading a button and its label separately
		// would make the label disappear faster than Unity's graphic tween.
		if (Widget != Row && (Widget->IsA<UImage>() || Widget->IsA<UTextBlock>()
			|| Widget->IsA<UBorder>() || Widget->IsA<UButton>()))
		{
			const float OriginalOpacity = Widget->GetRenderOpacity();
			if (OriginalOpacity > 0.0f)
			{
				TaskCompletedAnimatedGraphics.Add(Widget);
				TaskCompletedOriginalOpacities.Add(OriginalOpacity);
			}
			continue;
		}
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				if (UWidget* Child = Panel->GetChildAt(Index)) Descendants.Add(Child);
			}
		}
		else if (UUserWidget* Nested = Cast<UUserWidget>(Widget))
		{
			if (UWidget* Root = Nested->GetRootWidget()) Descendants.Add(Root);
		}
	}
	return true;
}

void URokUnityFeatureScreenWidget::ResetTaskCompletedRowAnimation()
{
	if (UCanvasPanel* Row = TaskCompletedAnimatedRow.Get()) Row->SetRenderScale(FVector2D(1.0f));
	for (int32 Index = 0; Index < TaskCompletedAnimatedGraphics.Num(); ++Index)
	{
		if (UWidget* Widget = TaskCompletedAnimatedGraphics[Index].Get())
		{
			Widget->SetRenderOpacity(TaskCompletedOriginalOpacities[Index]);
		}
	}
	TaskCompletedAnimatedRow.Reset();
	TaskCompletedAnimatedGraphics.Reset();
	TaskCompletedOriginalOpacities.Reset();
	TaskCompletedAnimationCallback = TFunction<void()>();
	TaskCompletedAnimationElapsed = -1.0f;
	TaskCompletedAnimationStartedAtSeconds = 0.0;
}

void URokUnityFeatureScreenWidget::AdvanceTaskCompletedRowAnimation()
{
	if (TaskCompletedAnimationElapsed < 0.0f) return;
	UCanvasPanel* Row = TaskCompletedAnimatedRow.Get();
	if (!IsValid(Row) || !Row->GetParent() || ActiveMode != ERokUnityFeatureScreenMode::Tasks
		|| SelectedTaskPage != 1)
	{
		ResetTaskCompletedRowAnimation();
		return;
	}
	// Unity uses SetUpdate(true); DOTweenSettings defaultEaseType 6 is
	// OutQuad. Monotonic wall time keeps the tween independent of game scale.
	const double Elapsed = FPlatformTime::Seconds() - TaskCompletedAnimationStartedAtSeconds;
	if (!FMath::IsFinite(Elapsed)) { ResetTaskCompletedRowAnimation(); return; }
	TaskCompletedAnimationElapsed = FMath::Clamp(static_cast<float>(Elapsed), 0.0f, 0.3f);
	const float Progress = TaskCompletedAnimationElapsed / 0.3f;
	const float Eased = 1.0f - FMath::Square(1.0f - Progress);
	Row->SetRenderScale(FVector2D(FMath::Lerp(1.0f, 0.1f, Eased)));
	for (int32 Index = 0; Index < TaskCompletedAnimatedGraphics.Num(); ++Index)
	{
		if (UWidget* Widget = TaskCompletedAnimatedGraphics[Index].Get())
		{
			Widget->SetRenderOpacity(TaskCompletedOriginalOpacities[Index] * (1.0f - Eased));
		}
	}
	if (Progress < 1.0f) return;
	for (int32 Index = 0; Index < TaskCompletedAnimatedGraphics.Num(); ++Index)
	{
		if (UWidget* Widget = TaskCompletedAnimatedGraphics[Index].Get())
		{
			Widget->SetRenderOpacity(TaskCompletedOriginalOpacities[Index]);
		}
	}
	TWeakObjectPtr<UCanvasPanel> CompletedRow = TaskCompletedAnimatedRow;
	TFunction<void()> OnComplete = MoveTemp(TaskCompletedAnimationCallback);
	TaskCompletedAnimatedRow.Reset();
	TaskCompletedAnimatedGraphics.Reset();
	TaskCompletedOriginalOpacities.Reset();
	TaskCompletedAnimationElapsed = -1.0f;
	TaskCompletedAnimationStartedAtSeconds = 0.0;
	if (CompletedRow.IsValid() && CompletedRow->GetParent() && OnComplete) OnComplete();
	if (UCanvasPanel* RemainingRow = CompletedRow.Get())
	{
		if (RemainingRow->GetParent()) RemainingRow->SetRenderScale(FVector2D(1.0f));
	}
}

namespace
{
FText FormatDailyCatalogTitle(int32 TaskId, const FRokUnityCatalogTask& Task)
{
	const FString DisplayName = Task.Name.IsEmpty() || Task.Name.Contains(TEXT("\uFFFD"))
		? TEXT("标题配置文本不可用") : Task.Name;
	return FText::FromString(FString::Printf(TEXT("#%d · %s"), TaskId, *DisplayName));
}

FText FormatDailyCatalogDescription(const FRokUnityCatalogTask& Task)
{
	const FString Description = Task.Desc.IsEmpty() || Task.Desc.Contains(TEXT("\uFFFD"))
		? TEXT("描述配置文本不可用")
		: ResolveTaskDescription(Task.Desc, Task.Param1, Task.Param2, Task.Require);
	return FText::FromString(Description);
}

FText FormatDailyCatalogTarget(const FRokUnityCatalogTask& Task)
{
	return FText::FromString(FString::Printf(
		TEXT("目标配置：类型 %d · 参数 %d/%d · 需求 %d"),
		Task.Type, Task.Param1, Task.Param2, Task.Require));
}

FText FormatDailyCatalogReward(const FRokUnityCatalogTask& Task)
{
	return FText::FromString(FString::Printf(
		TEXT("奖励配置组 #%d · 内容及可领取状态未验证"), Task.Reward));
}
}

#include "RokFeatureTaskDaily.inl"

int32 URokUnityFeatureScreenWidget::AddServerDailyTaskRows(const FRokServerTaskSnapshot& Snapshot,
	UScrollBox& Scroll, UClass* RowClass, UClass* RewardClass)
{
	int32 VisibleDailyCount = 0;
	for (const FRokServerTaskView& Task : Snapshot.DailyTasks)
	{
		// Unity removes claimed daily tasks from both visible task lists.
		if (Task.State == ERokServerTaskState::Received) continue;
		++VisibleDailyCount;
		FRokTaskRowState RowState;
		RowState.TaskId = Task.TaskId;
		RowState.Page = FRokTaskRowState::EPage::DailyTask;
		RowState.SourceAuthority = TEXT("server");
		RowState.State = Task.State == ERokServerTaskState::Received ? FRokTaskRowState::EState::Received
			: Task.State == ERokServerTaskState::Finished ? FRokTaskRowState::EState::Finished
			: Task.State == ERokServerTaskState::Unfinished ? FRokTaskRowState::EState::Unfinished
			: FRokTaskRowState::EState::NotSynced;
		RowState.Intent = RowState.State == FRokTaskRowState::EState::Finished
			? FRokTaskRowState::EIntent::Claim : RowState.State == FRokTaskRowState::EState::Unfinished
			? FRokTaskRowState::EIntent::Go : FRokTaskRowState::EIntent::Inspect;
		RowState.Type = Task.Type;
		RowState.Param1 = Task.Param1;
		RowState.Param2 = Task.Param2;
		RowState.Require = Task.Require;
		const int32 RowIndex = VisibleTaskStates.Add(RowState);
		const auto& Rewards = GetConfiguredRewardPreview(Task.RewardGroupId);
		Scroll.AddChild(WrapFixedSize(MakeTaskRow(RowIndex, Task.TaskId,
			FText::FromString(Task.Name),
			FText::FromString(Task.Description), FMath::Clamp<int64>(Task.Current, MIN_int32, MAX_int32),
			FMath::Clamp<int64>(Task.Target, MIN_int32, MAX_int32), Task.bProgressKnown,
			Task.State == ERokServerTaskState::Finished, ResolveTaskIconRect(Task.Icon),
			Task.RewardGroupId, Rewards, RowClass, RewardClass),
			*FString::Printf(TEXT("TaskDailyServerRowSize_%d"), Task.TaskId), FVector2D(1088.0f, 150.0f)));
	}
	return VisibleDailyCount;
}

void URokUnityFeatureScreenWidget::BuildTasks()
{
	const UGameInstance* TaskInstance = GetGameInstance();
	const URokServerTaskSubsystem* ServerTasks = !bTaskTestFixtureState && TaskInstance
		? TaskInstance->GetSubsystem<URokServerTaskSubsystem>() : nullptr;
	const FRokServerTaskSnapshot ServerSnapshot = ServerTasks
		? ServerTasks->GetSnapshot() : FRokServerTaskSnapshot();
	bLastServerTaskAvailable = ServerSnapshot.bAvailable;
	bLastServerTaskLoaded = ServerSnapshot.bLoaded;
	LastServerTaskSignature = ServerSnapshot.Signature;
	RenderedTaskChapterTaskIds.Reset();
	RenderedTaskChapterTaskRewardPackageIds.Reset();
	RenderedTaskChapterAggregateRewardPackageIds.Reset();
	VisibleTaskStates.Reset();
	bTaskMainLocalProjectionRendered = false;
	bTaskMainLocalConditionMet = false;
	TaskMainDisplayedCurrent = 0;

	URokUnityGameplayCatalogSubsystem* GameplayCatalog =
		GetGameplayCatalogSubsystem();
	if (GameplayCatalog && !GameplayCatalog->IsCatalogLoaded())
	{
		GameplayCatalog->LoadCatalog();
	}
	URokUnityRuntimeStateSubsystem* RuntimeState =
		GetRuntimeStateSubsystem();
	if (RuntimeState && !RuntimeState->IsRuntimeStateLoaded())
	{
		RuntimeState->LoadRuntimeState();
	}

	if (SelectedTaskPage == 2 && ServerSnapshot.bAvailable && TryBuildServerDailyTaskPage(ServerSnapshot))
	{
		return;
	}

	if (SelectedTaskPage == 0 && !ServerSnapshot.bAvailable)
	{
		UClass* RowClass = ResolveFeaturePageClass(TEXT("TaskChapterCatalogRow"));
		UClass* AggregateRewardClass = ResolveFeaturePageClass(TEXT("TaskChapterCatalogAggregateReward"));
		UClass* TaskRewardClass = ResolveFeaturePageClass(TEXT("TaskChapterCatalogTaskReward"));
		if (RowClass && AggregateRewardClass && TaskRewardClass)
		{
			if (const auto* Page = MountModePage(TEXT("TaskChapterCatalog")))
			{
				const auto* RewardHost = Page->Get<UCanvasPanel>(TEXT("TaskChapterAggregateRewardHost"));
				if (Page->Get<UCanvasPanel>(TEXT("TaskChapterRoot")) && Page->Get<UScrollBox>(TEXT("TaskChapterScroll"))
					&& RewardHost && Cast<UCanvasPanelSlot>(RewardHost->Slot))
				{
					for (int32 Index = 0; Index < 3; ++Index)
					{
						BindPageAction(*Page, *FString::Printf(TEXT("TaskPage_%d"), Index), 300 + Index);
					}
					BuildTaskChapter(Page, RowClass, AggregateRewardClass, TaskRewardClass);
					return;
				}
				ActiveModePage->RemoveFromParent();
				ActiveModePage = nullptr;
			}
		}
	}

	if (SelectedTaskPage == 2 && !ServerSnapshot.bAvailable)
	{
		UClass* RowClass = ResolveFeaturePageClass(TEXT("TaskDailyCatalogRow"));
		if (const URokUiPageWidget* Page = RowClass ? MountModePage(TEXT("TaskDailyCatalog")) : nullptr)
		{
			for (int32 Index = 0; Index < 3; ++Index)
			{
				BindPageAction(*Page, *FString::Printf(TEXT("TaskPage_%d"), Index), 300 + Index);
			}
			const bool bHasCatalog = GameplayCatalog && GameplayCatalog->IsCatalogLoaded();
			Page->SetPresent(Page->Get<UTextBlock>(TEXT("TaskPageEmpty")), !bHasCatalog);
			UScrollBox* DailyScroll = Page->Get<UScrollBox>(TEXT("TaskDailyCatalogScroll"));
			Page->SetPresent(DailyScroll, bHasCatalog);
			if (!bHasCatalog || !DailyScroll)
			{
				return;
			}
			UTexture2D* IconAtlas = LoadUiTexture(TaskIconAtlasPath);
			for (const int32 TaskId : GameplayCatalog->GetDailyTaskIds())
			{
				FRokUnityCatalogTask Task;
				if (!GameplayCatalog->GetDailyTask(TaskId, Task))
				{
					continue;
				}
				URokUiPageWidget* Row = CreateWidget<URokUiPageWidget>(this, RowClass);
				if (!Row)
				{
					continue;
				}
				Row->LegacyPrefix = TEXT("TaskDailyCatalog");
				Row->LegacySuffix = FString::Printf(TEXT("_%d"), TaskId);
				Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				DailyScroll->AddChild(Row);

				SetFeaturePageText(*Row, TEXT("Title"), FormatDailyCatalogTitle(TaskId, Task));
				SetFeaturePageText(*Row, TEXT("Description"), FormatDailyCatalogDescription(Task));
				SetFeaturePageText(*Row, TEXT("Target"), FormatDailyCatalogTarget(Task));
				SetFeaturePageText(*Row, TEXT("Reward"), FormatDailyCatalogReward(Task));
				if (UImage* Icon = Row->Get<UImage>(TEXT("Icon")))
				{
					Row->SetShown(Icon, IconAtlas != nullptr);
					if (IconAtlas)
					{
						FSlateBrush Brush = Icon->GetBrush();
						const FSlateBrush AtlasBrush = MakeAtlasBrush(IconAtlas, ResolveTaskIconRect(Task.Icon),
							TaskIconAtlasSize, Brush.GetImageSize());
						Brush.SetResourceObject(IconAtlas);
						Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
						Icon->SetBrush(Brush);
					}
				}
			}
			return;
		}
	}

	const bool bServerChapter = SelectedTaskPage == 0 && ServerSnapshot.bAvailable;
	if (bServerChapter && TryBuildServerTaskChapterPage(ServerSnapshot))
	{
		return;
	}
	const bool bServerTasks = ServerSnapshot.bAvailable;
	UClass* MainRowClass = SelectedTaskPage == 1 ? ResolveFeaturePageClass(
		bServerTasks ? TEXT("TaskDailyServerRow") : TEXT("TaskMainLocalRow")) : nullptr;
	UClass* MainRewardClass = MainRowClass ? ResolveFeaturePageClass(TEXT("TaskDailyServerReward")) : nullptr;
	URokUiPageWidget* MainPage = MainRewardClass ? MountModePage(
		bServerTasks ? TEXT("TaskMainServer") : TEXT("TaskMainLocal")) : nullptr;
	UScrollBox* TaskScroll = MainPage ? MainPage->Get<UScrollBox>(TEXT("TaskMainScroll")) : nullptr;
	UVerticalBox* MainContent = MainPage ? MainPage->Get<UVerticalBox>(TEXT("TaskMainContent")) : nullptr;
	UVerticalBox* SideContent = MainPage ? MainPage->Get<UVerticalBox>(TEXT("TaskSideContent")) : nullptr;
	if (MainPage && (!TaskScroll || !MainContent || !SideContent))
	{
		MainPage->RemoveFromParent();
		ActiveModePage = nullptr;
		MainPage = nullptr;
		TaskScroll = nullptr;
	}
	if (MainPage && bServerTasks)
	{
		BindServerTaskPageActions(*MainPage, ServerSnapshot);
	}
	else if (MainPage)
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			BindPageAction(*MainPage, *FString::Printf(TEXT("TaskPage_%d"), Index), 300 + Index);
		}
	}
	else
	{
		if (bServerChapter)
		{
			BuildSplitWindowFrame(CapturePoint(140, 80), FVector2D(1160, 756), GetServerTaskChapterTitle(ServerSnapshot),
				CapturePoint(1221, 74), nullptr, 1, 30);
			if (auto* Header = Cast<UTextBlock>(GetWidgetFromName(TEXT("FeatureFrameTitle"))))
			{
				Header->SetFont(URokMainHudWidget::MakeUnitySourceFont(30, false));
			}
		}
		else
		{
			BuildSplitWindowFrame(CapturePoint(146, 73), FVector2D(1148, 754),
				NSLOCTEXT("RokFeature", "TaskTitle", "任务"), CapturePoint(1230, 77));
			PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("TaskContentShade"), FLinearColor(0.065f, 0.07f, 0.07f, 0.98f)),
				CapturePoint(176, 136), FVector2D(1088, 664), 2);
		}
		static const FIntRect SelectedIcons[] = {SpriteRect(1, 313, 60, 60), SpriteRect(1, 437, 60, 60), SpriteRect(63, 499, 60, 60)};
		static const FIntRect DarkIcons[] = {SpriteRect(1, 375, 60, 60), SpriteRect(1, 499, 60, 60), SpriteRect(63, 561, 60, 60)};
		static const FText TabLabels[] = {
			NSLOCTEXT("RokFeature", "TaskChapterTab", "章节"),
			NSLOCTEXT("RokFeature", "TaskMainTab", "主线"),
			NSLOCTEXT("RokFeature", "TaskDailyTab", "每日")};
		const auto CountFinished = [](const auto& Rows)
		{
			return Algo::CountIf(Rows, [](const auto& Row) { return Row.State == ERokServerTaskState::Finished; });
		};
		const int32 RedCounts[] = {
			ServerSnapshot.bChapterClaimable ? 1 : CountFinished(ServerSnapshot.ChapterTasks),
			CountFinished(ServerSnapshot.MainTasks) + CountFinished(ServerSnapshot.SideTasks),
			CountFinished(ServerSnapshot.DailyTasks) + CountFinished(ServerSnapshot.DailyMilestones)
		};
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const bool bSelected = Index == SelectedTaskPage;
			const FVector2D TabRoot = CapturePoint(1258.0f, 212.0f + Index * 113.0f);
			const FVector2D TabVisualPosition = TabRoot + FVector2D(bSelected || bServerChapter ? 0.0f : 24.0f, 0.0f);
			const FVector2D TabVisualSize(bSelected ? 145.0f : 121.0f, 103.0f);
			PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("TaskPage_%d"), Index), 300 + Index, CommonAtlasPath, bSelected ? SideTabSelectedRect : SideTabDarkRect, CommonAtlasSize, TabVisualSize, FText::GetEmpty(), 20, FLinearColor::White, ESlateBrushDrawType::Image), TabVisualPosition, TabVisualSize, 7);
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("TaskPageIcon_%d"), Index), PageAtlasPath, bSelected ? SelectedIcons[Index] : DarkIcons[Index], PageAtlasSize, FVector2D(60.0f)), TabVisualPosition + FVector2D(31.0f, bServerChapter ? 22.0f : 20.0f), FVector2D(60.0f), 8);
			if (RedCounts[Index] > 0)
			{
				const FVector2D Dot = TabVisualPosition + FVector2D(bServerChapter ? 104.0f : TabVisualSize.X - (bSelected ? 41.0f : 24.0f), -1.0f);
				PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("TaskPageBadge_%d"), Index),
					CommonAtlasPath, RedPointRect, CommonAtlasSize, FVector2D(27, 29)), Dot, FVector2D(27, 29), 10);
				auto* Count = MakeScreenText(*FString::Printf(TEXT("TaskPageBadgeCount_%d"), Index),
					RedCounts[Index] > 99 ? 7 : 11, FLinearColor::White, true, ETextJustify::Center);
				Count->SetText(RedCounts[Index] > 99 ? FText::FromString(TEXT("99+")) : FText::AsNumber(RedCounts[Index]));
				PlaceOnCanvas(ModeCanvas, Count, Dot + FVector2D(0, RedCounts[Index] > 99 ? 7 : 3), FVector2D(27, 23), 11);
			}
			if (bServerChapter) continue;
			UTextBlock* TabLabel = MakeScreenText(
				*FString::Printf(TEXT("TaskPageLabel_%d"), Index),
				14,
				bSelected
					? FLinearColor::White
					: FLinearColor(0.68f, 0.68f, 0.66f, 1.0f),
				true,
				ETextJustify::Center);
			TabLabel->SetText(TabLabels[Index]);
			PlaceOnCanvas(
				ModeCanvas,
				TabLabel,
				TabVisualPosition + FVector2D(17.0f, 73.0f),
				FVector2D(92.0f, 24.0f),
				9);
		}
	}

	if (SelectedTaskPage == 0)
	{
		if (bServerChapter) BuildServerTaskChapter(ServerSnapshot);
		else BuildTaskChapter();
		return;
	}
	if (SelectedTaskPage != 1)
	{
		if (SelectedTaskPage == 2 && ServerSnapshot.bAvailable)
		{
			UScrollBox* DailyScroll = WidgetTree->ConstructWidget<UScrollBox>(
				UScrollBox::StaticClass(), TEXT("TaskDailyCatalogScroll"));
			DailyScroll->SetOrientation(Orient_Vertical);
			DailyScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
			DailyScroll->SetAllowOverscroll(true);
			DailyScroll->SetAnimateWheelScrolling(true);
			PlaceOnCanvas(ModeCanvas, DailyScroll, CapturePoint(176.0f, 136.0f), FVector2D(1088.0f, 664.0f), 5);
			UTextBlock* Header = MakeScreenText(TEXT("TaskDailyCatalogHeader"), 20,
				FLinearColor(0.83f, 0.79f, 0.65f, 1.0f));
			Header->SetText(ServerSnapshot.bLoaded && ServerSnapshot.bDailyLoaded
				? NSLOCTEXT("RokFeature", "TaskServerDailyHeader", "每日任务")
				: NSLOCTEXT("RokFeature", "TaskServerDailyWaiting", "等待服务器同步每日任务"));
			DailyScroll->AddChild(WrapFixedSize(Header, TEXT("TaskDailyCatalogHeaderSize"), FVector2D(1088.0f, 45.0f)));
			if (!ServerSnapshot.DailyMilestones.IsEmpty())
			{
				auto* Milestones = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TaskDailyMilestones"));
				auto* Points = MakeScreenText(TEXT("TaskDailyPoints"), 17, FLinearColor::White);
				Points->SetText(FText::Format(NSLOCTEXT("RokFeature", "TaskDailyActivePoints", "今日活跃度 {0}"),
					FText::AsNumber(ServerSnapshot.DailyActivePoints)));
				PlaceOnCanvas(Milestones, Points, FVector2D(18, 12), FVector2D(195, 40), 1);
				for (int32 Index = 0; Index < ServerSnapshot.DailyMilestones.Num(); ++Index)
				{
					const auto& Milestone = ServerSnapshot.DailyMilestones[Index];
					const bool bClaimable = Milestone.State == ERokServerTaskState::Finished;
					const TCHAR* Status = bClaimable ? TEXT("领取")
						: Milestone.State == ERokServerTaskState::Received ? TEXT("已领取") : TEXT("未达成");
					auto* Button = MakeActionButton(*FString::Printf(TEXT("TaskDailyMilestone_%d"), Milestone.Points),
						17800 + Index, CommonAtlasPath, bClaimable ? MiniGreenButtonRect : MiniBlueButtonRect,
						CommonAtlasSize, FVector2D(154, 45), FText::FromString(FString::Printf(TEXT("%d · %s"), Milestone.Points, Status)),
						16, FLinearColor::White, ESlateBrushDrawType::Box, MiniGreenMargin);
					Button->SetIsEnabled(bClaimable);
					TArray<FString> RewardText;
					for (const auto& Reward : GetConfiguredRewardPreview(Milestone.RewardGroupId))
						RewardText.Add(FString::Printf(TEXT("%s ×%lld"), *Reward.DisplayName, Reward.Quantity));
					Button->SetToolTipText(FText::FromString(FString::Join(RewardText, TEXT("、"))));
					PlaceOnCanvas(Milestones, Button, FVector2D(210 + Index * 166, 4), FVector2D(154, 45), 2);
				}
				DailyScroll->AddChild(WrapFixedSize(Milestones, TEXT("TaskDailyMilestonesSize"), FVector2D(1088, 58)));
			}
			const int32 VisibleDailyCount = AddServerDailyTaskRows(ServerSnapshot, *DailyScroll);
			if (ServerSnapshot.bDailyLoaded && VisibleDailyCount == 0)
			{
				UTextBlock* Empty = MakeScreenText(TEXT("TaskDailyServerEmpty"), 22, FLinearColor::White);
				Empty->SetText(NSLOCTEXT("RokFeature", "TaskServerDailyEmpty", "暂无每日任务"));
				DailyScroll->AddChild(WrapFixedSize(Empty, TEXT("TaskDailyServerEmptySize"), FVector2D(1088.0f, 90.0f)));
			}
			return;
		}
		if (SelectedTaskPage == 2 && GameplayCatalog && GameplayCatalog->IsCatalogLoaded())
		{
			UScrollBox* DailyScroll = WidgetTree->ConstructWidget<UScrollBox>(
				UScrollBox::StaticClass(), TEXT("TaskDailyCatalogScroll"));
			DailyScroll->SetOrientation(Orient_Vertical);
			DailyScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
			DailyScroll->SetAllowOverscroll(true);
			DailyScroll->SetAnimateWheelScrolling(true);
			DailyScroll->SetWheelScrollMultiplier(12.0f);
			PlaceOnCanvas(ModeCanvas, DailyScroll, CapturePoint(176.0f, 136.0f),
				FVector2D(1088.0f, 664.0f), 5);

			UTextBlock* DailyHeader = MakeScreenText(TEXT("TaskDailyCatalogHeader"), 20,
				FLinearColor(0.83f, 0.79f, 0.65f, 1.0f));
			DailyHeader->SetText(NSLOCTEXT("RokFeature", "TaskDailyCatalogHeader",
				"每日任务配置目录 · 非账户任务状态；进度、完成与领取状态未知"));
			DailyScroll->AddChild(WrapFixedSize(DailyHeader, TEXT("TaskDailyCatalogHeaderSize"),
				FVector2D(1088.0f, 45.0f)));

			const TArray<int32> DailyIds = GameplayCatalog->GetDailyTaskIds();
			for (const int32 TaskId : DailyIds)
			{
				FRokUnityCatalogTask Task;
				if (!GameplayCatalog->GetDailyTask(TaskId, Task))
				{
					continue;
				}
				UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(
					UCanvasPanel::StaticClass(), *FString::Printf(TEXT("TaskDailyCatalogRow_%d"), TaskId));
				Row->SetClipping(EWidgetClipping::ClipToBounds);
				PlaceOnCanvas(Row, MakeAtlasBorder(*FString::Printf(TEXT("TaskDailyCatalogBackground_%d"), TaskId),
					CommonAtlasPath, TaskRowRect, CommonAtlasSize, FVector2D(1026.0f, 140.0f), TaskRowMargin),
					FVector2D(31.0f, 2.0f), FVector2D(1026.0f, 140.0f));
				PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("TaskDailyCatalogIcon_%d"), TaskId),
					TaskIconAtlasPath, ResolveTaskIconRect(Task.Icon), TaskIconAtlasSize, FVector2D(90.0f, 98.0f)),
					FVector2D(62.0f, 22.0f), FVector2D(90.0f, 98.0f), 2);

				UTextBlock* Title = MakeScreenText(*FString::Printf(TEXT("TaskDailyCatalogTitle_%d"), TaskId),
					23, FLinearColor(0.87f, 0.78f, 0.49f, 1.0f), false);
				Title->SetText(FormatDailyCatalogTitle(TaskId, Task));
				PlaceOnCanvas(Row, Title, FVector2D(170.0f, 15.0f), FVector2D(820.0f, 29.0f), 3);
				UTextBlock* Description = MakeScreenText(*FString::Printf(TEXT("TaskDailyCatalogDescription_%d"), TaskId),
					19, FLinearColor(0.93f, 0.93f, 0.91f, 1.0f));
				Description->SetText(FormatDailyCatalogDescription(Task));
				PlaceOnCanvas(Row, Description, FVector2D(170.0f, 45.0f), FVector2D(820.0f, 27.0f), 3);
				UTextBlock* Target = MakeScreenText(*FString::Printf(TEXT("TaskDailyCatalogTarget_%d"), TaskId),
					17, FLinearColor(0.72f, 0.77f, 0.77f, 1.0f));
				Target->SetText(FormatDailyCatalogTarget(Task));
				PlaceOnCanvas(Row, Target, FVector2D(170.0f, 76.0f), FVector2D(820.0f, 24.0f), 3);
				UTextBlock* Reward = MakeScreenText(*FString::Printf(TEXT("TaskDailyCatalogReward_%d"), TaskId),
					17, FLinearColor(0.74f, 0.72f, 0.66f, 1.0f));
				Reward->SetText(FormatDailyCatalogReward(Task));
				PlaceOnCanvas(Row, Reward, FVector2D(170.0f, 106.0f), FVector2D(820.0f, 24.0f), 3);
				DailyScroll->AddChild(WrapFixedSize(Row,
					*FString::Printf(TEXT("TaskDailyCatalogRowSize_%d"), TaskId), FVector2D(1088.0f, 145.0f)));
			}
			return;
		}
		UTextBlock* Empty = MakeScreenText(TEXT("TaskPageEmpty"), 24, FLinearColor(0.72f, 0.72f, 0.7f, 1.0f), false, ETextJustify::Center);
		Empty->SetText(NSLOCTEXT("RokFeature", "TaskDailyCatalogUnavailable", "每日任务配置目录未加载"));
		PlaceOnCanvas(ModeCanvas, Empty, CapturePoint(370.0f, 395.0f), FVector2D(650.0f, 50.0f), 6);
		return;
	}

	if (!TaskScroll)
	{
		TaskScroll = WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(),
			TEXT("TaskMainScroll"));
		TaskScroll->SetOrientation(Orient_Vertical);
		TaskScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		TaskScroll->SetAllowOverscroll(true);
		TaskScroll->SetAnimateWheelScrolling(true);
		TaskScroll->SetWheelScrollMultiplier(12.0f);
		PlaceOnCanvas(
			ModeCanvas,
			TaskScroll,
			CapturePoint(176.0f, 136.0f),
			FVector2D(1088.0f, 664.0f),
			5);
	}

	auto AddTaskTag = [this, TaskScroll, MainPage](
		const FName& ContainerName,
		const FName& TextName,
		const FText& Label,
		const FLinearColor& Color)
	{
		if (MainPage)
		{
			SetFeaturePageText(*MainPage, TextName, Label);
			return;
		}
		UCanvasPanel* Tag = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			ContainerName);
		UTextBlock* TagText = MakeScreenText(TextName, 22, Color);
		TagText->SetText(Label);
		PlaceOnCanvas(Tag, TagText, FVector2D(35.0f, 0.0f), FVector2D(1018.0f, 30.0f), 1);
		TaskScroll->AddChild(WrapFixedSize(Tag, *FString::Printf(TEXT("%sSize"), *ContainerName.ToString()), FVector2D(1088.0f, 30.0f)));
	};

	TMap<int32, TArray<FRokTaskRewardPreviewRow>> RewardPreviewByGroup;
	TSet<int32> UnavailableRewardGroups;
	auto AddTaskRow = [this, TaskScroll, MainPage, MainContent, SideContent, MainRowClass, MainRewardClass,
		&RewardPreviewByGroup, &UnavailableRewardGroups](
		FRokTaskRowState RowState,
		const FText& Title,
		const FText& Description,
		int32 Current,
		int32 Target,
		bool bProgressKnown,
		const FIntRect& TaskIconRect,
		int32 RewardGroupId)
	{
		const int32 RowIndex = VisibleTaskStates.Num();
		VisibleTaskStates.Add(MoveTemp(RowState));
		const bool bServerRow = VisibleTaskStates[RowIndex].SourceAuthority == TEXT("server");
		if (!bServerRow && RewardGroupId > 0 && !RewardPreviewByGroup.Contains(RewardGroupId)
			&& !UnavailableRewardGroups.Contains(RewardGroupId))
		{
			TArray<FRokTaskRewardPreviewRow> Rows;
			FString Error;
			if (RokTaskRewardPreviewData::LoadGroup(RewardGroupId, Rows, Error))
			{
				RewardPreviewByGroup.Add(RewardGroupId, MoveTemp(Rows));
			}
			else
			{
				UnavailableRewardGroups.Add(RewardGroupId);
			}
		}
		const TArray<FRokTaskRewardPreviewRow>* PreviewRows = bServerRow
			? &GetConfiguredRewardPreview(RewardGroupId) : RewardPreviewByGroup.Find(RewardGroupId);
		const TArray<FRokTaskRewardPreviewRow> EmptyPreviewRows;
		UPanelWidget* TaskRows = TaskScroll;
		if (MainPage)
		{
			TaskRows = VisibleTaskStates[RowIndex].Page == FRokTaskRowState::EPage::MainTask ? MainContent : SideContent;
		}
		TaskRows->AddChild(WrapFixedSize(
			MakeTaskRow(
				RowIndex,
				VisibleTaskStates[RowIndex].TaskId,
				Title,
				Description,
				Current,
				Target,
				bProgressKnown,
				VisibleTaskStates[RowIndex].State == FRokTaskRowState::EState::Finished,
				TaskIconRect,
				RewardGroupId,
				PreviewRows ? *PreviewRows : EmptyPreviewRows,
				MainPage ? MainRowClass : nullptr, MainPage ? MainRewardClass : nullptr),
			*FString::Printf(TEXT("TaskRowSize_%d"), RowIndex),
			FVector2D(1088.0f, 150.0f)));
	};

	if (ServerSnapshot.bAvailable)
	{
		auto AddServerGroup = [this, &AddTaskTag, &AddTaskRow, &ServerSnapshot, TaskScroll, MainPage](
			const TArray<FRokServerTaskView>& Tasks, bool bMain)
		{
			AddTaskTag(bMain ? TEXT("TaskMainTag") : TEXT("TaskSideTag"),
				bMain ? TEXT("TaskMainHeader") : TEXT("TaskSideHeader"),
				bMain ? NSLOCTEXT("RokFeature", "MainTaskHeader", "主线任务")
					: NSLOCTEXT("RokFeature", "SideTaskHeader", "支线任务"),
				bMain ? FLinearColor(0.83f, 0.79f, 0.65f, 1.0f)
					: FLinearColor(0.56f, 0.76f, 0.85f, 1.0f));
			for (const FRokServerTaskView& Task : Tasks)
			{
				if (bMain && VisibleTaskStates.IsEmpty() && Task.bProgressKnown)
				{
					TaskMainDisplayedCurrent = static_cast<int32>(FMath::Clamp<int64>(Task.Current, MIN_int32, MAX_int32));
				}
				FRokTaskRowState Row;
				Row.TaskId = Task.TaskId;
				Row.Page = bMain ? FRokTaskRowState::EPage::MainTask : FRokTaskRowState::EPage::SideTask;
				Row.SourceAuthority = TEXT("server");
				Row.State = Task.State == ERokServerTaskState::Received ? FRokTaskRowState::EState::Received
					: Task.State == ERokServerTaskState::Finished ? FRokTaskRowState::EState::Finished
					: Task.State == ERokServerTaskState::Unfinished ? FRokTaskRowState::EState::Unfinished
					: FRokTaskRowState::EState::NotSynced;
				Row.Intent = Row.State == FRokTaskRowState::EState::Finished ? FRokTaskRowState::EIntent::Claim
					: Row.State == FRokTaskRowState::EState::Unfinished ? FRokTaskRowState::EIntent::Go
					: FRokTaskRowState::EIntent::Inspect;
				Row.Type = Task.Type;
				Row.Param1 = Task.Param1;
				Row.Param2 = Task.Param2;
				Row.Require = Task.Require;
				AddTaskRow(Row, FText::FromString(Task.Name),
					FText::FromString(Task.Description),
					static_cast<int32>(FMath::Clamp<int64>(Task.Current, MIN_int32, MAX_int32)),
					static_cast<int32>(FMath::Clamp<int64>(Task.Target, MIN_int32, MAX_int32)),
					Task.bProgressKnown, ResolveTaskIconRect(Task.Icon), Task.RewardGroupId);
			}
			const FName EmptyName = bMain ? TEXT("TaskMainEmpty") : TEXT("TaskSideNotSynced");
			const FName EmptySizeName = bMain ? TEXT("TaskMainEmptySize") : TEXT("TaskSideNotSyncedSize");
			if (MainPage && !Tasks.IsEmpty())
			{
				MainPage->SetPresent(MainPage->Get<USizeBox>(EmptySizeName), false);
			}
			if (Tasks.IsEmpty())
			{
				const FText EmptyText = !ServerSnapshot.bLoaded
					? NSLOCTEXT("RokFeature", "TaskServerWaiting", "等待服务器任务进度")
					: bMain ? NSLOCTEXT("RokFeature", "TaskServerMainEmpty", "暂无主线任务")
						: NSLOCTEXT("RokFeature", "TaskServerSideEmpty", "暂无支线任务");
				if (MainPage)
				{
					SetFeaturePageText(*MainPage, EmptyName, EmptyText);
				}
				else
				{
					UTextBlock* Empty = MakeScreenText(EmptyName, 22, FLinearColor(0.72f, 0.72f, 0.7f, 1.0f),
						false, ETextJustify::Center);
					Empty->SetText(EmptyText);
					TaskScroll->AddChild(WrapFixedSize(Empty, EmptySizeName, FVector2D(1088.0f, 90.0f)));
				}
			}
		};
		AddServerGroup(ServerSnapshot.MainTasks, true);
		AddServerGroup(ServerSnapshot.SideTasks, false);
		return;
	}

	AddTaskTag(
		TEXT("TaskMainTag"),
		TEXT("TaskMainHeader"),
		NSLOCTEXT("RokFeature", "MainTaskHeader", "主线任务"),
		FLinearColor(0.83f, 0.79f, 0.65f, 1.0f));

	const FRokUnityRuntimeTaskState Task = RuntimeState
		? RuntimeState->GetActiveTask()
		: FRokUnityRuntimeTaskState();
	if (MainPage)
	{
		MainPage->SetPresent(MainPage->Get<USizeBox>(TEXT("TaskMainEmptySize")), Task.TaskId <= 0);
		const bool bShowFixture = bTaskTestFixtureState && Task.TaskId > 0;
		MainPage->SetPresent(MainPage->Get<USizeBox>(TEXT("TaskCompletedAnimationFixtureSize")), bShowFixture);
		if (bShowFixture)
		{
			BindPageAction(*MainPage, TEXT("TaskCompletedAnimationPreview"), 319);
		}
	}
	if (Task.TaskId > 0)
	{
		const bool bProgressKnown =
			Task.ProgressRule.Equals(TEXT("building_level"), ESearchCase::IgnoreCase)
			|| Task.ProgressRule.Equals(TEXT("building_count"), ESearchCase::IgnoreCase);
		FRokTaskRowState MainRow;
		MainRow.TaskId = Task.TaskId;
		MainRow.Page = FRokTaskRowState::EPage::MainTask;
		MainRow.SourceAuthority = TEXT("runtime_bootstrap");
		MainRow.State = bProgressKnown && Task.bComplete
			? FRokTaskRowState::EState::Finished
			: FRokTaskRowState::EState::Unfinished;
		MainRow.Intent = MainRow.State == FRokTaskRowState::EState::Finished
			? FRokTaskRowState::EIntent::Claim
			: FRokTaskRowState::EIntent::Go;
		MainRow.Type = Task.Type;
		MainRow.Param1 = Task.Param1;
		MainRow.Param2 = Task.Param2;
		MainRow.Require = Task.Require;
		AddTaskRow(
			MainRow,
			FText::FromString(Task.Name),
			FText::FromString(ResolveTaskDescription(Task.Desc, Task.Param1, Task.Param2, Task.Require)),
			Task.Current,
			FMath::Max(1, Task.Target),
			bProgressKnown,
			ResolveTaskIconRect(Task.Icon),
			Task.Reward);
		RefreshTaskMainProjection();
	}
	else if (!MainPage)
	{
		UTextBlock* EmptyMain = MakeScreenText(TEXT("TaskMainEmpty"), 22, FLinearColor(0.72f, 0.72f, 0.7f, 1.0f), false, ETextJustify::Center);
		EmptyMain->SetText(NSLOCTEXT("RokFeature", "TaskMainEmpty", "暂无主线任务"));
		TaskScroll->AddChild(WrapFixedSize(EmptyMain, TEXT("TaskMainEmptySize"), FVector2D(1088.0f, 90.0f)));
	}
	if (!MainPage && bTaskTestFixtureState && Task.TaskId > 0)
	{
		UCanvasPanel* Preview = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("TaskCompletedAnimationFixturePanel"));
		UTextBlock* Notice = MakeScreenText(TEXT("TaskCompletedAnimationFixtureNotice"), 16,
			FLinearColor(0.78f, 0.82f, 0.85f, 1.0f));
		Notice->SetText(NSLOCTEXT("RokFeature", "TaskCompletedAnimationFixtureNotice", "本地完成行动画参考 · 不代表任务完成或奖励可领"));
		PlaceOnCanvas(Preview, Notice, FVector2D(32.0f, 10.0f), FVector2D(795.0f, 39.0f));
		PlaceOnCanvas(Preview, MakeActionButton(TEXT("TaskCompletedAnimationPreview"), 319,
			CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(202.0f, 54.0f),
			NSLOCTEXT("RokFeature", "TaskCompletedAnimationPreview", "完成行动画预览"), 17,
			FLinearColor::White, ESlateBrushDrawType::Box, MiniBlueMargin),
			FVector2D(838.0f, 0.0f), FVector2D(202.0f, 54.0f));
		TaskScroll->AddChild(WrapFixedSize(Preview, TEXT("TaskCompletedAnimationFixtureSize"),
			FVector2D(1088.0f, 58.0f)));
	}

	AddTaskTag(
		TEXT("TaskSideTag"),
		TEXT("TaskSideHeader"),
		bTaskTestFixtureState
			? NSLOCTEXT("RokFeature", "SideTaskHeaderFixture", "支线任务 · 本地引导参考态（非账号状态）")
			: NSLOCTEXT("RokFeature", "SideTaskHeader", "支线任务"),
		FLinearColor(0.56f, 0.76f, 0.85f, 1.0f));

	if (bTaskTestFixtureState)
	{
		const TArray<FRokUnityCatalogTask> SideCandidates = ComputeBootstrapSideTaskCandidates(
			GameplayCatalog,
			GetWorld() ? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr);
		if (MainPage)
		{
			MainPage->SetPresent(MainPage->Get<USizeBox>(TEXT("TaskSideNotSyncedSize")), false);
			MainPage->SetPresent(MainPage->Get<USizeBox>(TEXT("TaskSideEmptySize")), SideCandidates.IsEmpty());
		}
		for (const FRokUnityCatalogTask& Candidate : SideCandidates)
		{
			FRokTaskRowState SideRow;
			SideRow.TaskId = Candidate.Id;
			SideRow.Page = FRokTaskRowState::EPage::SideTask;
			SideRow.SourceAuthority = TEXT("local_fixture");
			SideRow.State = FRokTaskRowState::EState::Unfinished;
			SideRow.Intent = FRokTaskRowState::EIntent::Go;
			SideRow.Type = Candidate.Type;
			SideRow.Param1 = Candidate.Param1;
			SideRow.Param2 = Candidate.Param2;
			SideRow.Require = Candidate.Require;
			AddTaskRow(
				SideRow,
				FText::FromString(Candidate.Name),
				FText::FromString(ResolveTaskDescription(Candidate.Desc, Candidate.Param1, Candidate.Param2, Candidate.Require)),
				0,
				FMath::Max(1, Candidate.Require),
				true,
				ResolveTaskIconRect(Candidate.Icon),
				Candidate.Reward);
		}
		if (!MainPage && SideCandidates.IsEmpty())
		{
			UTextBlock* EmptySide = MakeScreenText(TEXT("TaskSideEmpty"), 22, FLinearColor(0.72f, 0.72f, 0.7f, 1.0f), false, ETextJustify::Center);
			EmptySide->SetText(NSLOCTEXT("RokFeature", "TaskSideEmpty", "暂无支线任务"));
			TaskScroll->AddChild(WrapFixedSize(EmptySide, TEXT("TaskSideEmptySize"), FVector2D(1088.0f, 90.0f)));
		}
		FString CandidateIds;
		for (const FRokUnityCatalogTask& Candidate : SideCandidates)
		{
			if (!CandidateIds.IsEmpty())
			{
				CandidateIds += TEXT(",");
			}
			CandidateIds += FString::Printf(TEXT("%d"), Candidate.Id);
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok tasks screen: page=main fixture=1 task=%d current=%d target=%d side_candidates=%d ids=%s authority=local_bootstrap_fixture."),
			Task.TaskId,
			Task.Current,
			Task.Target,
			SideCandidates.Num(),
			*CandidateIds);
		return;
	}

	// Production has no server side-task state yet; show the honest not-synced
	// state instead of presenting config rows as account tasks.
	if (MainPage)
	{
		MainPage->SetPresent(MainPage->Get<USizeBox>(TEXT("TaskSideEmptySize")), false);
		return;
	}
	UTextBlock* NotSyncedSide = MakeScreenText(TEXT("TaskSideNotSynced"), 22, FLinearColor(0.72f, 0.72f, 0.7f, 1.0f), false, ETextJustify::Center);
	NotSyncedSide->SetText(NSLOCTEXT("RokFeature", "TaskSideNotSynced", "支线任务进度尚未同步，连接服务器后更新。"));
	TaskScroll->AddChild(WrapFixedSize(NotSyncedSide, TEXT("TaskSideNotSyncedSize"), FVector2D(1088.0f, 90.0f)));

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok tasks screen: page=main task=%d current=%d target=%d side=not_synced authority=server."),
		Task.TaskId,
		Task.Current,
		Task.Target);
}

UWidget* URokUnityFeatureScreenWidget::MakeTaskRow(
	int32 RowIndex,
	int32 TaskId,
	const FText& Title,
	const FText& Description,
	int32 Current,
	int32 Target,
	bool bProgressKnown,
	bool bComplete,
	const FIntRect& TaskIconRect,
	int32 RewardGroupId,
	const TArray<FRokTaskRewardPreviewRow>& RewardPreviewRows,
	UClass* RowClass, UClass* RewardClass)
{
	const bool bServerRow = VisibleTaskStates.IsValidIndex(RowIndex)
		&& VisibleTaskStates[RowIndex].SourceAuthority == TEXT("server");
	FText RowDescription = bProgressKnown
		? FText::Format(
			NSLOCTEXT("RokFeature", "TaskProgressFormat", "{0} ({1}/{2})"),
			Description,
			FText::AsNumber(Current),
			FText::AsNumber(Target))
		: Description;
	if (bServerRow)
	{
		const FRokTaskRowState::EState RowState = VisibleTaskStates[RowIndex].State;
		const FText Status = RowState == FRokTaskRowState::EState::Received
			? NSLOCTEXT("RokFeature", "TaskServerReceivedStatus", "已领取")
			: RowState == FRokTaskRowState::EState::Finished
				? NSLOCTEXT("RokFeature", "TaskServerFinishedStatus", "已完成，待领取")
				: RowState == FRokTaskRowState::EState::Unfinished
					? NSLOCTEXT("RokFeature", "TaskServerUnfinishedStatus", "进行中")
					: NSLOCTEXT("RokFeature", "TaskServerUnknownStatus", "状态待同步");
		RowDescription = FText::Format(NSLOCTEXT("RokFeature", "TaskServerDescriptionStatus", "{0} · {1}{2}"),
			RowDescription, Status, bProgressKnown ? FText::GetEmpty()
				: NSLOCTEXT("RokFeature", "TaskServerProgressUnknown", " · 进度待同步"));
	}
	const FRokTaskRowState::EState DisplayState = VisibleTaskStates.IsValidIndex(RowIndex)
		? VisibleTaskStates[RowIndex].State : FRokTaskRowState::EState::NotSynced;
	const bool bCanShowClaim = bServerRow ? DisplayState == FRokTaskRowState::EState::Finished
		: bProgressKnown && bComplete;
	const FIntRect ButtonRect = bCanShowClaim ? MiniGreenButtonRect : MiniBlueButtonRect;
	const FMargin ButtonMargin = bCanShowClaim ? MiniGreenMargin : MiniBlueMargin;
	const FText ButtonLabel = bServerRow && DisplayState == FRokTaskRowState::EState::Received
		? NSLOCTEXT("RokFeature", "TaskServerReceivedButton", "已领取")
		: bServerRow && DisplayState == FRokTaskRowState::EState::NotSynced
			? NSLOCTEXT("RokFeature", "TaskServerUnknownButton", "状态待同步")
		: (bServerRow ? bCanShowClaim : bComplete)
			? NSLOCTEXT("RokFeature", "TaskClaimReward", "领取奖励")
			: NSLOCTEXT("RokFeature", "TaskGo", "前往");
	if (RowClass && RewardClass)
	{
		if (auto* RowPage = BuildTaskRowPage(RowClass, RewardClass, RowIndex, Title, RowDescription,
			TaskIconRect, RewardGroupId, RewardPreviewRows, bServerRow, bCanShowClaim, ButtonLabel))
		{
			return RowPage;
		}
	}

	UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("TaskRow_%d"), RowIndex));
	Row->SetClipping(EWidgetClipping::ClipToBounds);
	PlaceOnCanvas(Row, MakeAtlasBorder(*FString::Printf(TEXT("TaskRowBackground_%d"), RowIndex), CommonAtlasPath, TaskRowRect, CommonAtlasSize, FVector2D(1026.0f, 140.0f), TaskRowMargin), FVector2D(31.0f, 2.0f), FVector2D(1026.0f, 140.0f));
	PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("TaskIconQuality_%d"), RowIndex), Item0AtlasPath, ItemQualityLegendaryRect, Item0AtlasSize, FVector2D(100.0f)), FVector2D(60.0f, 22.0f), FVector2D(100.0f), 2);
	PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("TaskIcon_%d"), RowIndex), TaskIconAtlasPath, TaskIconRect, TaskIconAtlasSize, FVector2D(90.0f, 98.0f)), FVector2D(65.0f, 23.0f), FVector2D(90.0f, 98.0f), 3);
	UTextBlock* TitleText = MakeScreenText(*FString::Printf(TEXT("TaskTitle_%d"), RowIndex), 24, FLinearColor(0.87f, 0.78f, 0.49f, 1.0f), false);
	TitleText->SetText(Title);
	PlaceOnCanvas(Row, TitleText, FVector2D(175.0f, 18.0f),
		bServerRow ? FVector2D(815.0f, 30.0f) : FVector2D(623.0f, 28.0f), 3);
	UTextBlock* DescriptionText = MakeScreenText(*FString::Printf(TEXT("TaskDescription_%d"), RowIndex), 20, FLinearColor(0.93f, 0.93f, 0.91f, 1.0f));
	DescriptionText->SetText(RowDescription);
	if (bServerRow)
	{
		// Keep the source title scale and leave room for the appended server progress.
		TitleText->SetFont(URokMainHudWidget::MakeUnitySourceFont(24, false));
		DescriptionText->SetFont(URokMainHudWidget::MakeUnitySourceFont(22, false));
		DescriptionText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		DescriptionText->SetToolTipText(RowDescription);
		DescriptionText->SetVisibility(ESlateVisibility::Visible);
	}
	PlaceOnCanvas(Row, DescriptionText, FVector2D(175.0f, bServerRow ? 49.0f : 45.0f),
		FVector2D(bServerRow ? 867.0f : 632.0f, 28.0f), 3);
	PlaceOnCanvas(Row, MakeAtlasBorder(*FString::Printf(TEXT("TaskRewardStrip_%d"), RowIndex), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(664.0f, 43.0f), CommonPanelMargin), FVector2D(167.0f, 79.0f), FVector2D(664.0f, 43.0f), 2);
	UTextBlock* RewardLabel = MakeScreenText(*FString::Printf(TEXT("TaskRewardLabel_%d"), RowIndex), 17, FLinearColor(0.74f, 0.72f, 0.66f, 1.0f));
	RewardLabel->SetText(NSLOCTEXT("RokFeature", "TaskReward", "奖励"));
	PlaceOnCanvas(Row, RewardLabel, FVector2D(180.0f, 86.0f), FVector2D(79.0f, 28.0f), 4);

	if (!RewardPreviewRows.IsEmpty())
	{
		constexpr int32 VisibleRewardSlots = 4;
		const bool bOverflow = RewardPreviewRows.Num() > VisibleRewardSlots;
		const int32 VisibleRewardCount = bOverflow ? VisibleRewardSlots - 1 : RewardPreviewRows.Num();
		for (int32 Index = 0; Index < VisibleRewardCount; ++Index)
		{
			const FRokTaskRewardPreviewRow& Preview = RewardPreviewRows[Index];
			const FString BaseName = FString::Printf(TEXT("TaskRewardPreview_%d_%d"), RowIndex, Index);
			UCanvasPanel* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *BaseName);
			const FText Quantity = FText::AsNumber(Preview.Quantity);
			Cell->SetToolTipText(FText::Format(
				bServerRow
					? NSLOCTEXT("RokFeature", "TaskServerRewardPreviewTooltip", "{0} ×{1} · 奖励配置预览，领取以服务器为准")
					: NSLOCTEXT("RokFeature", "TaskRewardPreviewTooltip", "{0} ×{1} · 任务奖励预览，服务器状态待同步"),
				FText::FromString(Preview.DisplayName), Quantity));
			const bool bHasIcon = Preview.IconRect.Width() > 0 && Preview.IconRect.Height() > 0;
			if (bHasIcon)
			{
				PlaceOnCanvas(Cell, MakeAtlasImage(*(BaseName + TEXT("Icon")),
					Preview.AtlasPath.IsEmpty() ? CommonAtlasPath : *Preview.AtlasPath, Preview.IconRect,
					Preview.IconAtlasSize, FVector2D(34.0f)), FVector2D::ZeroVector, FVector2D(34.0f), 1);
			}
			UTextBlock* Count = MakeScreenText(*(BaseName + TEXT("Count")), 17,
				FLinearColor(0.95f, 0.9f, 0.75f, 1.0f), false);
			Count->SetText(bHasIcon ? Quantity : FText::Format(NSLOCTEXT("RokFeature", "TaskRewardTextOnly", "{0} ×{1}"),
				FText::FromString(Preview.DisplayName), Quantity));
			PlaceOnCanvas(Cell, Count, FVector2D(bHasIcon ? 38.0f : 0.0f, 3.0f), FVector2D(bHasIcon ? 92.0f : 152.0f, 27.0f), 2);
			PlaceOnCanvas(Row, Cell, FVector2D(268.0f + Index * 134.0f, 83.0f), FVector2D(132.0f, 36.0f), 5);
		}
		if (bOverflow)
		{
			auto* More = MakeScreenText(*FString::Printf(TEXT("TaskRewardMore_%d"), RowIndex), 16, FLinearColor::White);
			More->SetText(FText::Format(NSLOCTEXT("RokFeature", "TaskRewardMore", "另有 {0} 项"),
				FText::AsNumber(RewardPreviewRows.Num() - VisibleRewardCount)));
			TArray<FString> Items;
			for (int32 Index = VisibleRewardCount; Index < RewardPreviewRows.Num(); ++Index)
			{
				const auto& Reward = RewardPreviewRows[Index];
				Items.Add(FString::Printf(TEXT("%s ×%lld"), *Reward.DisplayName, Reward.Quantity));
			}
			More->SetToolTipText(FText::FromString(FString::Join(Items, TEXT("、"))));
			More->SetVisibility(ESlateVisibility::Visible);
			PlaceOnCanvas(Row, More, FVector2D(268.0f + VisibleRewardCount * 134.0f, 86.0f), FVector2D(132.0f, 29.0f), 5);
		}
	}
	else if (RewardGroupId > 0)
	{
		UTextBlock* Missing = MakeScreenText(*FString::Printf(TEXT("TaskRewardUnavailable_%d"), RowIndex), 16,
			FLinearColor(0.74f, 0.72f, 0.66f, 1.0f));
		Missing->SetText(NSLOCTEXT("RokFeature", "TaskRewardPreviewUnavailable", "奖励预览暂无来源数据"));
		PlaceOnCanvas(Row, Missing, FVector2D(269.0f, 86.0f), FVector2D(360.0f, 28.0f), 4);
	}

	PlaceOnCanvas(
		Row,
		MakeActionButton(
			*FString::Printf(TEXT("TaskAction_%d"), RowIndex),
			bServerRow ? 6000 + RowIndex : 320 + RowIndex,
			CommonAtlasPath,
			ButtonRect,
			CommonAtlasSize,
			FVector2D(202.0f, 54.0f),
			ButtonLabel,
			21,
			FLinearColor::White,
			ESlateBrushDrawType::Box,
			ButtonMargin),
		FVector2D(838.0f, 74.0f),
		FVector2D(202.0f, 54.0f),
		6);
	return Row;
}

void URokUnityFeatureScreenWidget::RefreshServerPlayerAssetsPresentation()
{
	const UGameInstance* Instance = GetGameInstance();
	if (!Instance) return;
	if (ActiveMode == ERokUnityFeatureScreenMode::Commander)
	{
		const URokServerHeroSubsystem* Server = Instance->GetSubsystem<URokServerHeroSubsystem>();
		const FRokServerHeroSnapshot Snapshot = Server ? Server->GetSnapshot() : FRokServerHeroSnapshot();
		const auto* Inventory = Instance->GetSubsystem<URokServerInventorySubsystem>();
		const FString InventorySignature = Snapshot.bAvailable && Inventory ? Inventory->GetSnapshot().Signature : FString();
		if (Snapshot.bAvailable == bLastServerHeroAvailable
			&& Snapshot.bHeroesKnown == bLastServerHeroKnown
			&& Snapshot.Signature == LastServerHeroSignature
			&& InventorySignature == LastCommanderInventorySignature) return;
		float Offset = 0.0f;
		if (const UScrollBox* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("CommanderRosterScroll"))))
			Offset = Scroll->GetScrollOffset();
		RebuildModeContent();
		if (UScrollBox* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("CommanderRosterScroll"))))
			Scroll->SetScrollOffset(Offset);
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::Items)
	{
		const URokServerInventorySubsystem* Server = Instance->GetSubsystem<URokServerInventorySubsystem>();
		const FRokServerInventorySnapshot Snapshot = Server ? Server->GetSnapshot() : FRokServerInventorySnapshot();
		if (Snapshot.bAvailable == bLastServerInventoryAvailable
			&& Snapshot.bItemsKnown == bLastServerInventoryKnown
			&& Snapshot.Signature == LastServerInventorySignature) return;
		float Offset = 0.0f;
		if (const UScrollBox* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ItemsInventoryScroll"))))
			Offset = Scroll->GetScrollOffset();
		RebuildModeContent();
		if (UScrollBox* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ItemsInventoryScroll"))))
			Scroll->SetScrollOffset(Offset);
	}
}

UTexture2D* URokUnityFeatureScreenWidget::LoadCommanderPreviewTexture(const FString& RelativePath)
{
	auto& Texture = CommanderPreviewTextures.FindOrAdd(RelativePath);
	if (!Texture)
		Texture = FImageUtils::ImportFileAsTexture2D(RokDataPaths::Resolve(TEXT("Commander/") + RelativePath));
	return Texture;
}

void URokUnityFeatureScreenWidget::ShowCommanderScene(const FString& Scene)
{
	const FName SceneName(*Scene);
	if (CommanderSceneName == SceneName)
	{
		// Roster and detail refreshes keep the currently playing background.
		CommanderSceneImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}
	StopCommanderSceneMotion();
	if (UTexture2D* Still = LoadCommanderPreviewTexture(TEXT("Scenes/") + Scene + TEXT(".png")))
	{
		CommanderSceneImage->SetBrushFromTexture(Still);
		CommanderSceneImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	CommanderSceneName = SceneName;

#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokCommanderStaticCapture")))
	{
		return;
	}
#endif
	if (!FApp::CanEverRender()) return;
	const FString Movie = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()
		/ TEXT("Movies/Commander") / (Scene + TEXT(".mp4")));
	if (!IFileManager::Get().FileExists(*Movie)) return;
	if (!CommanderScenePlayer)
	{
		CommanderScenePlayer = NewObject<UMediaPlayer>(this);
		CommanderScenePlayer->PlayOnOpen = true;
		CommanderScenePlayer->SetLooping(true);
		CommanderSceneVideo = NewObject<UMediaTexture>(this);
		CommanderSceneVideo->NewStyleOutput = true;
		CommanderSceneVideo->SetMediaPlayer(CommanderScenePlayer);
		CommanderSceneVideo->UpdateResource();
	}
	if (!CommanderScenePlayer->OpenFile(Movie))
		UE_LOG(LogTemp, Warning, TEXT("Commander motion could not open %s"), *Scene);
}

void URokUnityFeatureScreenWidget::StopCommanderSceneMotion()
{
	if (CommanderScenePlayer && !CommanderSceneName.IsNone()) CommanderScenePlayer->Close();
	CommanderSceneName = NAME_None;
	if (CommanderSceneImage) CommanderSceneImage->SetBrushResourceObject(nullptr);
}

void URokUnityFeatureScreenWidget::UpdateCommanderSceneMotion()
{
	if (!CommanderScenePlayer || !CommanderSceneVideo || !CommanderSceneImage
		|| CommanderSceneImage->GetBrush().GetResourceObject() == CommanderSceneVideo
		|| !CommanderScenePlayer->IsPlaying() || CommanderScenePlayer->GetTime() <= FTimespan::Zero()
		|| CommanderSceneVideo->GetWidth() <= 1) return;

	CommanderSceneImage->SetBrushResourceObject(CommanderSceneVideo);
	CommanderSceneImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	UE_LOG(LogTemp, Display, TEXT("Commander motion playing: %s (%dx%d, %.2fs)"),
		*CommanderSceneName.ToString(), CommanderSceneVideo->GetWidth(), CommanderSceneVideo->GetHeight(),
		CommanderScenePlayer->GetDuration().GetTotalSeconds());
}

#include "RokFeatureCommander.inl"

#include "RokFeatureCommanderProgression.inl"

void URokUnityFeatureScreenWidget::PrepareCommanderCommand(FRokHeroCommand Request)
{
	Request.HeroId = SelectedCommanderId;
	auto* Server = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
	FRokHeroCommandCost Cost;
	FString Error;
	bool bReady = Server && Server->GetCommandCost(Request, Cost, Error);
	if (!bReady && Server && (Request.Type == ERokHeroCommand::SwitchTalent || Request.Type == ERokHeroCommand::ResetTalent))
	{
		int64 Items = 0;
		if (Cost.ItemCount > 0 && Server->GetItemCount(Cost.ItemId, Items) && Items < Cost.ItemCount)
		{
			Request.bUseGems = true;
			bReady = Server->GetCommandCost(Request, Cost, Error);
		}
	}
	if (!bReady)
	{
		if (Cost.ItemCount > 0 && Cost.AvailableItems < Cost.ItemCount && Error == TEXT("所需道具不足。"))
			Error = FString::Printf(TEXT("所需道具不足（%lld/%d）。"), Cost.AvailableItems, Cost.ItemCount);
		ShowBoundaryFeedback(FText::FromString(Error.IsEmpty() ? TEXT("统帅数据尚未同步。") : Error));
		return;
	}
	CommanderConfirmation = MoveTemp(Request);
	CommanderConfirmationCost = Cost;
	RebuildModeContent();
}

void URokUnityFeatureScreenWidget::ConfirmCommanderCommand()
{
	if (!CommanderConfirmation.IsSet()) return;
	auto* Server = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
	const FRokHeroCommand Request = *CommanderConfirmation;
	FRokHeroCommandCost Cost;
	FString Error;
	if (!Server || !Server->GetCommandCost(Request, Cost, Error))
	{
		ShowBoundaryFeedback(FText::FromString(Error.IsEmpty() ? TEXT("统帅数据尚未同步。") : Error));
		return;
	}
	if (Cost.ItemId != CommanderConfirmationCost.ItemId || Cost.ItemCount != CommanderConfirmationCost.ItemCount
		|| Cost.Gems != CommanderConfirmationCost.Gems)
	{
		CommanderConfirmationCost = Cost;
		RebuildModeContent();
		ShowBoundaryFeedback(FText::FromString(TEXT("操作费用已更新，请重新确认。")));
		return;
	}
	const TWeakObjectPtr<URokUnityFeatureScreenWidget> WeakThis(this);
	const bool bSent = Server->SubmitCommand(Request, [WeakThis, HeroId = Request.HeroId](const FString& Result)
	{
		auto* Widget = WeakThis.Get();
		if (!Widget || Widget->ActiveMode != ERokUnityFeatureScreenMode::Commander
			|| Widget->SelectedCommanderId != HeroId) return;
		Widget->RefreshServerPlayerAssetsPresentation();
		Widget->ShowBoundaryFeedback(FText::FromString(Result.IsEmpty() ? TEXT("服务器已确认统帅操作。") : Result));
	}, Error);
	if (bSent)
	{
		CommanderConfirmation.Reset();
		RebuildModeContent();
	}
	ShowBoundaryFeedback(FText::FromString(bSent ? TEXT("等待服务器确认统帅操作…") : Error));
}

bool URokUnityFeatureScreenWidget::HandleCommanderProgressionAction(int32 ActionId)
{
	FRokHeroCommand Request;
	Request.Page = CommanderTalentPage;
	switch (ActionId)
	{
	case 450:
		Request.Type = ERokHeroCommand::UpgradeSkill;
		break;
	case 451:
	{
		const auto* Server = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
		const auto Snapshot = Server ? Server->GetSnapshot() : FRokServerHeroSnapshot();
		const auto* Hero = Snapshot.Heroes.FindByPredicate([this](const auto& Row) { return Row.HeroId == SelectedCommanderId; });
		CommanderTalentPage = Hero && Hero->bTalentIndexKnown ? FMath::Clamp(Hero->TalentIndex, 1, 3) : 1;
		bCommanderTalentOpen = true;
		CommanderConfirmation.Reset();
		RebuildModeContent();
		return true;
	}
	case 452:
		Request.Type = ERokHeroCommand::Summon;
		break;
	case 19800: case 19801: case 19802:
		if (!bCommanderTalentOpen) return true;
		CommanderTalentPage = ActionId - 19800 + 1;
		CommanderConfirmation.Reset();
		RebuildModeContent();
		return true;
	case 19803:
		bCommanderTalentOpen = false;
		CommanderConfirmation.Reset();
		RebuildModeContent();
		return true;
	case 19804:
		if (!bCommanderTalentOpen) return true;
		Request.Type = ERokHeroCommand::SwitchTalent;
		break;
	case 19805:
		if (!bCommanderTalentOpen) return true;
		Request.Type = ERokHeroCommand::ResetTalent;
		break;
	case 19806:
	{
		const auto* Input = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("CommanderTalentNameInput")));
		if (!bCommanderTalentOpen || !Input) return true;
		Request.Type = ERokHeroCommand::RenameTalent;
		Request.Name = Input->GetText().ToString().TrimStartAndEnd();
		break;
	}
	case 19807:
		ConfirmCommanderCommand();
		return true;
	case 19808:
		CommanderConfirmation.Reset();
		RebuildModeContent();
		return true;
	default:
		if (!bCommanderTalentOpen || !CommanderTalentNodes.IsValidIndex(ActionId - 19820)) return false;
		Request.Type = ERokHeroCommand::LearnTalent;
		Request.TalentId = CommanderTalentNodes[ActionId - 19820].Id;
		break;
	}
	PrepareCommanderCommand(MoveTemp(Request));
	return true;
}

void URokUnityFeatureScreenWidget::AdvanceCommanderRosterAnimation(float DeltaSeconds)
{
	CommanderRosterAnimationTime = FMath::Min(RokCommanderRosterAnimation::Duration,
		CommanderRosterAnimationTime + FMath::Max(0.0f, DeltaSeconds));
	// CaptainMediator.LoadFinish opens both source panels together. Sorting and
	// hero selection only rebuild presentation; they must not restart entry.
	const auto DataSample = RokCommanderDataAnimation::Evaluate(CommanderRosterAnimationTime);
	for (const auto& WeakWidget : CommanderDataAnimationWidgets)
	{
		if (UWidget* Widget = WeakWidget.Get())
		{
			Widget->SetRenderTranslation(FVector2D(DataSample.X, 0));
			Widget->SetRenderOpacity(DataSample.Opacity);
		}
	}
	const auto Sample = RokCommanderRosterAnimation::Evaluate(CommanderRosterAnimationTime);
	for (const auto& WeakWidget : CommanderRosterAnimationWidgets)
	{
		if (UWidget* Widget = WeakWidget.Get())
		{
			Widget->SetRenderTranslation(FVector2D(Sample.X, 0));
			Widget->SetRenderOpacity(Sample.Opacity);
		}
	}
}

#include "RokFeatureItems.inl"

URokUnityFeatureScreenWidget::FVipPresentation URokUnityFeatureScreenWidget::ReadVipPresentation() const
{
	FVipPresentation State;
	const URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
	if (bVipTestFixtureState)
	{
		State.CurrentLevel = 0;
		State.Points = 0;
		State.LoginDays = 1;
		State.LevelThreshold = 200;
		State.TodayPoints = 40;
		State.DailyClaimed = false;
		State.FreeChestClaimed = false;
	}
	else
	{
		if (const URokUnityRuntimeStateSubsystem* Runtime = GetRuntimeStateSubsystem())
		{
			const FRokUnityRuntimePlayerProfileState Profile = Runtime->GetPlayerProfileState();
			if (Profile.bHasVipLevel)
			{
				State.CurrentLevel = FMath::Clamp(Profile.VipLevel, 0, VipLevelCount - 1);
			}
		}
		const UGameInstance* Instance = GetGameInstance();
		const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
		const auto Role = Net && Net->GetState() == ERokGameNetState::RoleReady ? Net->GetRoleInfo() : nullptr;
		if (!Role) return State;

		RokSprotoJson::TryGetInt64(Role, TEXT("vip"), State.Points);
		RokSprotoJson::TryGetInt64(Role, TEXT("continuousLoginDay"), State.LoginDays);
		bool bClaimed = false;
		if (Role->TryGetBoolField(TEXT("vipExpFlag"), bClaimed)) State.DailyClaimed = bClaimed;
		if (Role->TryGetBoolField(TEXT("vipFreeBox"), bClaimed)) State.FreeChestClaimed = bClaimed;
		const TArray<TSharedPtr<FJsonValue>>* Purchased = nullptr;
		if (Role->TryGetArrayField(TEXT("vipSpecialBox"), Purchased))
		{
			for (const auto& Value : *Purchased)
			{
				double Level = 0.0;
				if (Value && Value->TryGetNumber(Level) && Level >= 0 && Level < VipLevelCount)
				{
					State.PurchasedLevels |= 1u << static_cast<int32>(Level);
				}
			}
		}
		State.bClaimPending = Net->IsVipClaimPending();
	}

	if (Catalog && State.Points >= 0)
	{
		// UI_Win_VipMediator uses the first cumulative threshold above the account's points.
		for (int32 Level = 0; Level < VipLevelCount; ++Level)
		{
			FRokUnityCatalogVipLevel Definition;
			if (Catalog->GetVipLevel(Level, Definition)
				&& (Definition.Point > State.Points || Level == VipLevelCount - 1))
			{
				State.CurrentLevel = Level;
				State.LevelThreshold = Definition.Point;
				break;
			}
		}
	}
	if (Catalog && State.LoginDays >= 0)
	{
		FRokUnityCatalogVipDayPoint Day;
		if (State.LoginDays == 0)
		{
			State.TodayPoints = 0;
		}
		else if (Catalog->GetVipDayPoint(static_cast<int32>(FMath::Min<int64>(State.LoginDays, MAX_int32)), Day))
		{
			State.TodayPoints = Day.Point;
		}
	}
	return State;
}

#include "RokFeatureVip.inl"

void URokUnityFeatureScreenWidget::RefreshVipPresentation()
{
	const FVipPresentation State = ReadVipPresentation();
	if (State == LastVipPresentation) return;
	if (State.CurrentLevel != INDEX_NONE
		&& SelectedVipPreviewLevel == FMath::Max(0, LastVipPresentation.CurrentLevel))
	{
		SelectedVipPreviewLevel = State.CurrentLevel;
	}
	RebuildModeContent();
}

void URokUnityFeatureScreenWidget::RequestVipClaim(bool bFreeChest)
{
	if (bVipTestFixtureState)
	{
		ShowBoundaryFeedback(NSLOCTEXT("RokVip", "FixtureClaim", "当前为 VIP 本地参考预览。"));
		return;
	}
	if (bFreeChest && SelectedVipPreviewLevel != ReadVipPresentation().CurrentLevel)
	{
		return;
	}
	UGameInstance* Instance = GetGameInstance();
	auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	FString Error;
	const TWeakObjectPtr<URokUnityFeatureScreenWidget> WeakThis(this);
	if (Net && Net->RequestVipClaim(bFreeChest, [WeakThis](const FString& Failure)
	{
		if (auto* Screen = WeakThis.Get(); Screen && Screen->ActiveMode == ERokUnityFeatureScreenMode::Vip)
		{
			Screen->RefreshVipPresentation();
			Screen->ShowBoundaryFeedback(Failure.IsEmpty()
				? NSLOCTEXT("RokVip", "ClaimSucceeded", "领取成功")
				: FText::FromString(Failure));
		}
	}, Error))
	{
		return;
	}
	ShowBoundaryFeedback(Error.IsEmpty()
		? NSLOCTEXT("RokVip", "ClaimRequiresServer", "请登录后领取 VIP 奖励。")
		: FText::FromString(Error));
}

void URokUnityFeatureScreenWidget::BuildAlliance()
{
	if (bAllianceRankingReferenceOpen)
	{
		BuildAllianceRankingReference();
		return;
	}
	if (bAllianceTerritoryReferenceOpen)
	{
		BuildAllianceTerritoryReference();
		return;
	}
	if (bAllianceShopReferenceOpen)
	{
		BuildAllianceShopReference();
		return;
	}
	if (bAllianceGiftReferenceOpen)
	{
		BuildAllianceGiftReference();
		UE_LOG(LogTemp, Display, TEXT("Rok alliance gift reference: static_config_only=1 account_state=0 claim_state=0."));
		return;
	}
	if (bAllianceTechnologyReferenceOpen)
	{
		BuildAllianceTechnologyReference();
		UE_LOG(LogTemp, Display,
			TEXT("Rok alliance technology reference: static_catalog_only=1 alliance_state_unchanged=1."));
		return;
	}
	if (bAllianceOfficeReferenceOpen)
	{
		BuildAllianceOfficeReference();
		UE_LOG(LogTemp, Display,
			TEXT("Rok alliance screen: page=office_reference fixture=%d authority=local_reference state_unchanged=1."),
			bAllianceTestFixtureState ? 1 : 0);
		return;
	}
	const TCHAR* PageToken = TEXT("welcome");
	switch (AlliancePreviewState)
	{
	case 1:
		PageToken = TEXT("join");
		BuildAllianceJoinList();
		break;
	case 2:
		PageToken = TEXT("joined");
		BuildAllianceJoined();
		break;
	case 3:
		PageToken = bAllianceFlagEditorOpen ? TEXT("flag_local_edit") : TEXT("create");
		if (bAllianceFlagEditorOpen) BuildAllianceFlagEditor();
		else BuildAllianceCreate();
		break;
	default:
		BuildAllianceWelcome();
		break;
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok alliance screen: page=%s fixture=%d authority=%s."),
		PageToken,
		bAllianceTestFixtureState ? 1 : 0,
		bAllianceTestFixtureState ? TEXT("local_fixture") : TEXT("server"));
}

void URokUnityFeatureScreenWidget::BuildAllianceWelcome()
{
	if (const URokUiPageWidget* Page = MountModePage(TEXT("AllianceWelcome")))
	{
		BindPageAction(*Page, TEXT("AllianceWelcomeJoin"), 600);
		BindPageAction(*Page, TEXT("AllianceWelcomeCreate"), 601);
		BindPageAction(*Page, TEXT("AllianceTechnologyReferenceOpen"), 636);
		BindPageAction(*Page, TEXT("AllianceOfficeReferenceOpen"), 633);
		return;
	}
	BuildSplitWindowFrame(CapturePoint(170.0f, 73.0f), FVector2D(1100.0f, 754.0f), NSLOCTEXT("RokAlliance", "Title", "联盟"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceWelcomeShade"), FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)), CapturePoint(184.0f, 153.0f), FVector2D(1072.0f, 664.0f), 2);
	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceWelcomeBanner"), AllianceAtlasPath, AllianceJoinBannerRect, AllianceAtlasSize, FVector2D(1016.0f, 312.0f)), CapturePoint(212.0f, 164.0f), FVector2D(1016.0f, 312.0f), 4);
	UTextBlock* BannerMotto = MakeScreenText(TEXT("AllianceWelcomeBannerMotto"), 36, FLinearColor::White, true, ETextJustify::Center);
	BannerMotto->SetText(NSLOCTEXT("RokAlliance", "WelcomeBannerMotto", "团结\n荣誉\n信念\n征服"));
	PlaceOnCanvas(ModeCanvas, BannerMotto, CapturePoint(226.0f, 202.0f), FVector2D(112.0f, 242.0f), 6);

	static const FText BenefitLabels[] = {
		NSLOCTEXT("RokAlliance", "FirstJoinReward", "首次加入联盟将获得300钻石"),
		NSLOCTEXT("RokAlliance", "AllianceHelp", "联盟援建将为您节约大量的时间"),
		NSLOCTEXT("RokAlliance", "MutualHelp", "盟友之间应该互相鼓励和帮助"),
		NSLOCTEXT("RokAlliance", "Fortress", "建立联盟要塞来进行领土扩张"),
		NSLOCTEXT("RokAlliance", "Occupation", "占领古城和关卡来获得收益"),
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(BenefitLabels); ++Index)
	{
		const float Y = 511.0f + Index * 49.0f;
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("AllianceBenefitStar_%d"), Index), AllianceAtlasPath, AllianceStarRect, AllianceAtlasSize, FVector2D(38.0f)), CapturePoint(210.0f, Y - 4.0f), FVector2D(38.0f), 5);
		UTextBlock* Benefit = MakeScreenText(*FString::Printf(TEXT("AllianceBenefit_%d"), Index), 19, FLinearColor(0.91f, 0.91f, 0.89f, 1.0f), false);
		Benefit->SetText(BenefitLabels[Index]);
		PlaceOnCanvas(ModeCanvas, Benefit, CapturePoint(263.0f, Y), FVector2D(520.0f, 32.0f), 6);
	}
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("AllianceWelcomeJoin"), 600, CommonAtlasPath, MediumBlueButtonRect, CommonAtlasSize, FVector2D(202.0f, 81.0f), NSLOCTEXT("RokAlliance", "Join", "加入"), 29, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f)),
		CapturePoint(995.0f, 505.0f),
		FVector2D(202.0f, 81.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("AllianceWelcomeCreate"), 601, CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize, FVector2D(202.0f, 81.0f), NSLOCTEXT("RokAlliance", "Create", "创建"), 29, FLinearColor(0.23f, 0.16f, 0.04f, 1.0f)),
		CapturePoint(995.0f, 670.0f),
		FVector2D(202.0f, 81.0f),
		7);
	PlaceOnCanvas(ModeCanvas,
		MakeActionButton(TEXT("AllianceTechnologyReferenceOpen"), 636, CommonAtlasPath,
			MiniBlueButtonRect, CommonAtlasSize, FVector2D(202.0f, 48.0f),
			NSLOCTEXT("RokAlliance", "TechnologyReferenceOpen", "科技配置参考"), 18,
			FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(780.0f, 762.0f), FVector2D(202.0f, 48.0f), 7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("AllianceOfficeReferenceOpen"), 633, CommonAtlasPath,
			MiniBlueButtonRect, CommonAtlasSize, FVector2D(202.0f, 48.0f),
			NSLOCTEXT("RokAlliance", "OfficeReferenceOpen", "官职配置参考"), 18,
			FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(995.0f, 762.0f), FVector2D(202.0f, 48.0f), 7);
}

void URokUnityFeatureScreenWidget::BuildAllianceJoinList()
{
	static const TCHAR* AllianceNames[] = {TEXT("[SNOW] 雪原同盟"), TEXT("[ROK] 王国远征军"), TEXT("[CN1] 九州盟约"), TEXT("[SUN] 日曜骑士团"), TEXT("[WLD] 世界之巅")};
	static const TCHAR* AllianceLeaders[] = {TEXT("snow"), TEXT("远征者"), TEXT("九州牧"), TEXT("Sunrise"), TEXT("WorldKing")};
	static const TCHAR* AlliancePower[] = {TEXT("6,100"), TEXT("25,730"), TEXT("18,420"), TEXT("13,880"), TEXT("9,650")};
	static const TCHAR* AllianceMembers[] = {TEXT("1/50"), TEXT("42/50"), TEXT("35/50"), TEXT("27/50"), TEXT("19/50")};
	const FString& SearchKey = AllianceFixtureSearchQuery;
	if (bAllianceTestFixtureState)
	{
		VisibleAllianceFixtureIndices.Reset();
		if (SearchKey.IsEmpty() || SearchKey.Len() >= 3)
		{
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(AllianceNames); ++Index)
			{
				// Case-insensitive matching is a local fixture rule, not a verified server query rule.
				if (SearchKey.IsEmpty() || FString(AllianceNames[Index]).Contains(SearchKey, ESearchCase::IgnoreCase))
				{
					VisibleAllianceFixtureIndices.Add(Index);
				}
			}
		}
		if (!VisibleAllianceFixtureIndices.Contains(SelectedAllianceListIndex))
		{
			SelectedAllianceListIndex = VisibleAllianceFixtureIndices.IsEmpty()
				? INDEX_NONE : VisibleAllianceFixtureIndices[0];
		}
	}

	const auto FormatStats = [](int32 Index)
	{
		return FText::FromString(FString::Printf(TEXT("战力 %s    成员 %s    中文"), AlliancePower[Index], AllianceMembers[Index]));
	};
	const auto FormatDetailLeader = [](int32 Index)
	{
		return FText::FromString(FString::Printf(TEXT("盟主：%s\n战力：%s\n成员：%s"),
			AllianceLeaders[Index], AlliancePower[Index], AllianceMembers[Index]));
	};
	const auto FormatJoinType = [](int32 Index)
	{
		return Index == 0 ? NSLOCTEXT("RokAlliance", "DirectJoin", "任何人均可加入")
			: NSLOCTEXT("RokAlliance", "ApplyJoin", "申请加入");
	};
	const auto FormatConfirm = [](int32 Index)
	{
		return Index == 0 ? NSLOCTEXT("RokAlliance", "JoinNow", "加入") : NSLOCTEXT("RokAlliance", "ApplyNow", "申请加入");
	};
	const auto FormatFixtureEmpty = [&SearchKey]()
	{
		return SearchKey.Len() < 3
			? NSLOCTEXT("RokAlliance", "FixtureSearchMinimum", "本地参考搜索请至少输入3个字符。")
			: NSLOCTEXT("RokAlliance", "FixtureSearchNoMatch", "本地参考列表没有匹配的联盟。");
	};

	UClass* RowClass = ResolveFeaturePageClass(TEXT("AllianceJoinRow"));
	UClass* SelectedRowClass = ResolveFeaturePageClass(TEXT("AllianceJoinRowSelected"));
	if (const URokUiPageWidget* Page = RowClass && SelectedRowClass ? MountModePage(TEXT("AllianceJoin")) : nullptr)
	{
		BindPageAction(*Page, TEXT("AllianceSearch"), 624);
		BindPageAction(*Page, TEXT("AllianceJoinBack"), 621);
		const bool bFixture = bAllianceTestFixtureState;
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceJoinEmptyState")), !bFixture);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceJoinServiceState")), !bFixture);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceJoinFixtureTag")), bFixture);
		Page->SetPresent(Page->Get<UBorder>(TEXT("AllianceJoinDetailBg")), bFixture);
		const bool bHasSelection = bFixture && SelectedAllianceListIndex != INDEX_NONE;
		for (const FName WidgetName : {FName(TEXT("AllianceJoinDetailFlag")), FName(TEXT("AllianceJoinDetailName")),
			FName(TEXT("AllianceJoinDetailLeader")), FName(TEXT("AllianceJoinNotice")), FName(TEXT("AllianceJoinConfirm"))})
		{
			Page->SetPresent(Page->Get<UWidget>(WidgetName), bHasSelection);
		}
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceJoinDetailEmpty")), bFixture && !bHasSelection);
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceJoinFixtureEmpty")),
			bFixture && VisibleAllianceFixtureIndices.IsEmpty());
		if (!bFixture)
		{
			return;
		}

		if (UEditableTextBox* Input = Page->Get<UEditableTextBox>(TEXT("AllianceSearchInput")))
		{
			Input->SetText(FText::FromString(AllianceFixtureSearchInput));
		}
		for (int32 SlotIndex = 0; SlotIndex < VisibleAllianceFixtureIndices.Num(); ++SlotIndex)
		{
			const int32 Index = VisibleAllianceFixtureIndices[SlotIndex];
			UCanvasPanel* RowSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("AllianceJoinRowSlot_%d"), SlotIndex));
			UClass* StyleClass = Index == SelectedAllianceListIndex ? SelectedRowClass : RowClass;
			if (const URokUiPageWidget* Row = MountPageItem(RowSlot, StyleClass, TEXT("AllianceJoin"), Index))
			{
				SetFeaturePageText(*Row, TEXT("Name"), FText::FromString(AllianceNames[Index]));
				SetFeaturePageText(*Row, TEXT("Stats"), FormatStats(Index));
				SetFeaturePageText(*Row, TEXT("Type"), FormatJoinType(Index));
				BindPageAction(*Row, TEXT("RowHit"), 610 + Index);
			}
		}
		if (bHasSelection)
		{
			const int32 Index = SelectedAllianceListIndex;
			SetFeaturePageText(*Page, TEXT("AllianceJoinDetailName"), FText::FromString(AllianceNames[Index]));
			SetFeaturePageText(*Page, TEXT("AllianceJoinDetailLeader"), FormatDetailLeader(Index));
			SetFeaturePageText(*Page, TEXT("AllianceJoinConfirmLabel"), FormatConfirm(Index));
			BindPageAction(*Page, TEXT("AllianceJoinConfirm"), 620);
		}
		else
		{
			SetFeaturePageText(*Page, TEXT("AllianceJoinFixtureEmpty"), FormatFixtureEmpty());
		}
		return;
	}

	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f), NSLOCTEXT("RokAlliance", "JoinAllianceTitle", "加入联盟"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceJoinShade"), FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)), CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);

	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceSearchBack"), FLinearColor(0.20f, 0.20f, 0.20f, 1.0f)), CapturePoint(198.0f, 169.0f), FVector2D(720.0f, 46.0f), 4);
	UEditableTextBox* SearchInput =
		WidgetTree->ConstructWidget<UEditableTextBox>(
			UEditableTextBox::StaticClass(),
			TEXT("AllianceSearchInput"));
	SearchInput->SetHintText(NSLOCTEXT(
		"RokAlliance",
		"SearchHint",
		"点击此处输入想要搜索的联盟..."));
	FEditableTextBoxStyle SearchStyle = FEditableTextBoxStyle::GetDefault();
	SearchStyle
		.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18))
		.SetForegroundColor(FLinearColor::White)
		.SetFocusedForegroundColor(FLinearColor::White)
		.SetBackgroundColor(FLinearColor(0.20f, 0.20f, 0.20f, 1.0f))
		.SetPadding(FMargin(12.0f, 6.0f));
	SearchInput->SetWidgetStyle(SearchStyle);
	SearchInput->SetClearKeyboardFocusOnCommit(false);
	if (bAllianceTestFixtureState)
	{
		SearchInput->SetText(FText::FromString(AllianceFixtureSearchInput));
	}
	PlaceOnCanvas(
		ModeCanvas,
		SearchInput,
		CapturePoint(205.0f, 171.0f),
		FVector2D(705.0f, 42.0f),
		5);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceSearch"), 624, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(94.0f, 46.0f), NSLOCTEXT("RokAlliance", "Search", "搜索"), 18, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(928.0f, 169.0f), FVector2D(94.0f, 46.0f), 5);
	if (!bAllianceTestFixtureState)
	{
		// Unity shows Language 730024 for an empty recommended list; the
		// service line keeps the no-server state explicit instead of implying
		// a real kingdom query happened.
		UTextBlock* EmptyState = MakeScreenText(
			TEXT("AllianceJoinEmptyState"),
			24,
			FLinearColor(0.78f, 0.77f, 0.72f, 1.0f),
			false,
			ETextJustify::Center);
		EmptyState->SetText(NSLOCTEXT(
			"RokAlliance",
			"JoinEmptyState",
			"当前王国未找到合适的联盟"));
		PlaceOnCanvas(
			ModeCanvas,
			EmptyState,
			CapturePoint(270.0f, 405.0f),
			FVector2D(900.0f, 60.0f),
			5);
		UTextBlock* ServiceState = MakeScreenText(
			TEXT("AllianceJoinServiceState"),
			18,
			FLinearColor(0.62f, 0.62f, 0.58f, 1.0f),
			false,
			ETextJustify::Center);
		ServiceState->SetText(NSLOCTEXT(
			"RokAlliance",
			"JoinServiceState",
			"联盟列表需要服务器数据，连接服务器后更新。"));
		PlaceOnCanvas(
			ModeCanvas,
			ServiceState,
			CapturePoint(270.0f, 465.0f),
			FVector2D(900.0f, 40.0f),
			5);
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				TEXT("AllianceJoinBack"),
				621,
				CommonAtlasPath,
				MiniBlueButtonRect,
				CommonAtlasSize,
				FVector2D(138.0f, 54.0f),
				NSLOCTEXT("RokAlliance", "Back", "返回"),
				18,
				FLinearColor(0.04f, 0.14f, 0.24f, 1.0f),
				ESlateBrushDrawType::Box,
				MiniBlueMargin),
			CapturePoint(208.0f, 735.0f),
			FVector2D(138.0f, 54.0f),
			7);
		return;
	}

	// Alliance lists are server data; fixture rows must stay explicitly
	// labeled instead of reading as account/kingdom state.
	UTextBlock* JoinFixtureTag = MakeScreenText(TEXT("AllianceJoinFixtureTag"), 15, FLinearColor(0.62f, 0.62f, 0.58f, 1.0f), false, ETextJustify::Right);
	JoinFixtureTag->SetText(NSLOCTEXT("RokAlliance", "JoinFixtureTag", "联盟列表 · 本地参考态（非账号状态）"));
	PlaceOnCanvas(ModeCanvas, JoinFixtureTag, CapturePoint(760.0f, 742.0f), FVector2D(500.0f, 26.0f), 6);
	if (VisibleAllianceFixtureIndices.IsEmpty())
	{
		UTextBlock* EmptyList = MakeScreenText(TEXT("AllianceJoinFixtureEmpty"), 20,
			FLinearColor(0.78f, 0.77f, 0.72f, 1.0f), false, ETextJustify::Center);
		EmptyList->SetText(FormatFixtureEmpty());
		PlaceOnCanvas(ModeCanvas, EmptyList, CapturePoint(198.0f, 410.0f), FVector2D(670.0f, 60.0f), 5);
	}
	for (int32 VisibleIndex = 0; VisibleIndex < VisibleAllianceFixtureIndices.Num(); ++VisibleIndex)
	{
		const int32 Index = VisibleAllianceFixtureIndices[VisibleIndex];
		const float Y = 232.0f + VisibleIndex * 92.0f;
		const bool bSelected = Index == SelectedAllianceListIndex;
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(*FString::Printf(TEXT("AllianceJoinRowBg_%d"), Index), bSelected ? FLinearColor(0.72f, 0.68f, 0.53f, 0.96f) : FLinearColor(0.17f, 0.17f, 0.17f, 0.96f)), CapturePoint(198.0f, Y), FVector2D(670.0f, 82.0f), 4);
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("AllianceJoinFlag_%d"), Index), AllianceFlagAtlasPath, AllianceFlagClothRect, AllianceFlagAtlasSize, FVector2D(58.0f, 68.0f)), CapturePoint(208.0f, Y + 7.0f), FVector2D(58.0f, 68.0f), 5);
		UTextBlock* Name = MakeScreenText(*FString::Printf(TEXT("AllianceJoinName_%d"), Index), 20, bSelected ? FLinearColor(0.18f, 0.14f, 0.08f, 1.0f) : FLinearColor(0.93f, 0.91f, 0.85f, 1.0f), true);
		Name->SetText(FText::FromString(AllianceNames[Index]));
		PlaceOnCanvas(ModeCanvas, Name, CapturePoint(278.0f, Y + 8.0f), FVector2D(320.0f, 30.0f), 6);
		UTextBlock* Stats = MakeScreenText(*FString::Printf(TEXT("AllianceJoinStats_%d"), Index), 16, bSelected ? FLinearColor(0.25f, 0.22f, 0.16f, 1.0f) : FLinearColor(0.78f, 0.78f, 0.75f, 1.0f), false);
		Stats->SetText(FormatStats(Index));
		PlaceOnCanvas(ModeCanvas, Stats, CapturePoint(278.0f, Y + 43.0f), FVector2D(430.0f, 28.0f), 6);
		UTextBlock* JoinType = MakeScreenText(*FString::Printf(TEXT("AllianceJoinType_%d"), Index), 16, bSelected ? FLinearColor(0.25f, 0.22f, 0.16f, 1.0f) : FLinearColor(0.78f, 0.78f, 0.75f, 1.0f), false, ETextJustify::Right);
		JoinType->SetText(FormatJoinType(Index));
		PlaceOnCanvas(ModeCanvas, JoinType, CapturePoint(690.0f, Y + 27.0f), FVector2D(158.0f, 28.0f), 6);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("AllianceJoinRowHit_%d"), Index), 610 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(670.0f, 82.0f)), CapturePoint(198.0f, Y), FVector2D(670.0f, 82.0f), 9);
	}

	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceJoinDetailBg"), FLinearColor(0.25f, 0.24f, 0.21f, 0.98f)), CapturePoint(888.0f, 232.0f), FVector2D(350.0f, 474.0f), 4);
	if (SelectedAllianceListIndex != INDEX_NONE)
	{
		const int32 DetailIndex = SelectedAllianceListIndex;
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceJoinDetailFlag"), AllianceFlagAtlasPath, AllianceFlagClothRect, AllianceFlagAtlasSize, FVector2D(116.0f, 136.0f)), CapturePoint(1005.0f, 250.0f), FVector2D(116.0f, 136.0f), 5);
		UTextBlock* DetailName = MakeScreenText(TEXT("AllianceJoinDetailName"), 22, FLinearColor(0.94f, 0.91f, 0.80f, 1.0f), true, ETextJustify::Center);
		DetailName->SetText(FText::FromString(AllianceNames[DetailIndex]));
		PlaceOnCanvas(ModeCanvas, DetailName, CapturePoint(905.0f, 396.0f), FVector2D(316.0f, 38.0f), 6);
		UTextBlock* DetailLeader = MakeScreenText(TEXT("AllianceJoinDetailLeader"), 18, FLinearColor(0.82f, 0.82f, 0.79f, 1.0f), false, ETextJustify::Center);
		DetailLeader->SetText(FormatDetailLeader(DetailIndex));
		PlaceOnCanvas(ModeCanvas, DetailLeader, CapturePoint(930.0f, 442.0f), FVector2D(266.0f, 84.0f), 6);
		UTextBlock* Notice = MakeScreenText(TEXT("AllianceJoinNotice"), 17, FLinearColor(0.90f, 0.90f, 0.87f, 1.0f), false, ETextJustify::Center);
		Notice->SetText(NSLOCTEXT("RokAlliance", "AllianceNotice", "团结互助，共同建设联盟领土。"));
		Notice->SetAutoWrapText(true);
		PlaceOnCanvas(ModeCanvas, Notice, CapturePoint(920.0f, 540.0f), FVector2D(286.0f, 60.0f), 6);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceJoinConfirm"), 620, CommonAtlasPath, MediumBlueButtonRect, CommonAtlasSize, FVector2D(202.0f, 81.0f), FormatConfirm(DetailIndex), 26, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f)), CapturePoint(960.0f, 615.0f), FVector2D(202.0f, 81.0f), 7);
	}
	else
	{
		UTextBlock* EmptyDetail = MakeScreenText(TEXT("AllianceJoinDetailEmpty"), 17,
			FLinearColor(0.82f, 0.82f, 0.79f, 1.0f), false, ETextJustify::Center);
		EmptyDetail->SetText(NSLOCTEXT("RokAlliance", "FixtureSelectEmpty", "本地参考列表暂无可选联盟。"));
		PlaceOnCanvas(ModeCanvas, EmptyDetail, CapturePoint(908.0f, 442.0f), FVector2D(310.0f, 54.0f), 6);
	}
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceJoinBack"), 621, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f), NSLOCTEXT("RokAlliance", "Back", "返回"), 18, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(208.0f, 735.0f), FVector2D(138.0f, 54.0f), 7);
}

#include "RokFeatureAllianceRanking.inl"

#include "RokFeatureAllianceTerritory.inl"

#include "RokFeatureAllianceShop.inl"

#include "RokFeatureAllianceGift.inl"

void URokUnityFeatureScreenWidget::BuildAllianceOfficeReference()
{
	URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
	const bool bDefinitionsLoaded = Catalog && Catalog->LoadAllianceOfficeReadonly();
	const TArray<FRokUnityAllianceOfficeReadonlyRow>* OfficeRows = bDefinitionsLoaded
		? &Catalog->GetAllianceOfficeReadonlyRows() : nullptr;
	const auto FormatName = [](const FRokUnityAllianceOfficeReadonlyRow& Office)
	{
		return Office.Name.IsEmpty()
			? FText::FromString(FString::Printf(TEXT("#%d 官职名称配置不可用"), Office.Id))
			: FText::FromString(FString::Printf(TEXT("#%d · %s"), Office.Id, *Office.Name));
	};
	const auto FormatDescription = [](const FRokUnityAllianceOfficeReadonlyRow& Office)
	{
		return Office.Description.IsEmpty()
			? NSLOCTEXT("RokAlliance", "OfficeDescriptionUnavailable", "职位说明配置不可用")
			: FText::FromString(Office.Description);
	};
	const auto FormatBonus = [](const FRokUnityAllianceOfficeReadonlyRow& Office)
	{
		return Office.AddDescription.IsEmpty()
			? NSLOCTEXT("RokAlliance", "OfficeBonusUnavailable", "配置加成说明不可用")
			: FText::FromString(FString::Printf(TEXT("配置加成说明：%s"), *Office.AddDescription));
	};

	UClass* RowClass = ResolveFeaturePageClass(TEXT("AllianceOfficeRow"));
	UClass* SelectedRowClass = ResolveFeaturePageClass(TEXT("AllianceOfficeRowSelected"));
	if (const URokUiPageWidget* Page = RowClass && SelectedRowClass ? MountModePage(TEXT("AllianceOffice")) : nullptr)
	{
		const bool bHasRows = OfficeRows && OfficeRows->Num() == 4;
		Page->SetPresent(Page->Get<UTextBlock>(TEXT("AllianceOfficeReferenceUnavailable")), !bHasRows);
		if (bHasRows)
		{
			for (int32 Index = 0; Index < OfficeRows->Num(); ++Index)
			{
				const FRokUnityAllianceOfficeReadonlyRow& Office = (*OfficeRows)[Index];
				UCanvasPanel* RowSlot = Page->Get<UCanvasPanel>(*FString::Printf(TEXT("AllianceOfficeReferenceRowSlot_%d"), Index));
				UClass* StyleClass = Index == SelectedAllianceOfficeIndex ? SelectedRowClass : RowClass;
				const URokUiPageWidget* Row = MountPageItem(RowSlot, StyleClass, TEXT("AllianceOfficeReference"), Office.Id);
				if (!Row)
				{
					continue;
				}
				SetFeaturePageText(*Row, TEXT("Name"), FormatName(Office));
				SetFeaturePageText(*Row, TEXT("Description"), FormatDescription(Office));
				SetFeaturePageText(*Row, TEXT("Bonus"), FormatBonus(Office));
				if (UImage* Icon = Row->Get<UImage>(TEXT("Icon")))
				{
					const FIntRect IconRect = AllianceOfficeIconRect(Office.IconSprite);
					if (IconRect.Width() <= 0 || IconRect.Height() <= 0)
					{
						Row->SetPresent(Icon, false);
					}
					else if (UTexture2D* Texture = LoadUiTexture(AllianceAtlasPath))
					{
						FSlateBrush Brush = Icon->GetBrush();
						const FSlateBrush AtlasBrush = MakeAtlasBrush(Texture, IconRect, AllianceAtlasSize, Brush.GetImageSize());
						Brush.SetResourceObject(Texture);
						Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
						Icon->SetBrush(Brush);
					}
					else
					{
						Row->SetShown(Icon, false);
					}
				}
				BindPageAction(*Row, TEXT("Hit"), 640 + Index);
			}
		}
		BindPageAction(*Page, TEXT("AllianceOfficeReferenceBack"), 634);
		return;
	}

	BuildSplitWindowFrame(CapturePoint(170.0f, 73.0f), FVector2D(1100.0f, 754.0f),
		NSLOCTEXT("RokAlliance", "OfficeReferenceTitle", "官职配置参考"), CapturePoint(1230.0f, 77.0f));
	UTextBlock* Intro = MakeScreenText(TEXT("AllianceOfficeReferenceIntro"), 16,
		FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
	Intro->SetText(NSLOCTEXT("RokAlliance", "OfficeReferenceBoundary",
		"仅展示配置定义，不代表当前任命、人数、权限或已生效加成；选择不会改变账号状态。"));
	PlaceOnCanvas(ModeCanvas, Intro, CapturePoint(210.0f, 158.0f), FVector2D(1010.0f, 34.0f), 6);
	if (OfficeRows && OfficeRows->Num() == 4)
	{
		for (int32 Index = 0; Index < OfficeRows->Num(); ++Index)
		{
			const FRokUnityAllianceOfficeReadonlyRow& Office = (*OfficeRows)[Index];
			const float Y = 235.0f + Index * 122.0f;
			const bool bSelected = Index == SelectedAllianceOfficeIndex;
			PlaceOnCanvas(ModeCanvas,
				MakeSolidBorder(*FString::Printf(TEXT("AllianceOfficeReferenceRow_%d"), Office.Id),
					bSelected ? FLinearColor(0.34f, 0.31f, 0.24f, 0.98f)
					: FLinearColor(0.16f, 0.16f, 0.16f, 0.96f)),
				CapturePoint(198.0f, Y), FVector2D(1040.0f, 118.0f), 4);
			const FIntRect IconRect = AllianceOfficeIconRect(Office.IconSprite);
			if (IconRect.Width() > 0 && IconRect.Height() > 0)
			{
				PlaceOnCanvas(ModeCanvas, MakeAtlasImage(
					*FString::Printf(TEXT("AllianceOfficeReferenceIcon_%d"), Office.Id),
					AllianceAtlasPath, IconRect, AllianceAtlasSize, FVector2D(72.0f)),
					CapturePoint(210.0f, Y + 12.0f), FVector2D(72.0f), 5);
			}
			UTextBlock* Name = MakeScreenText(*FString::Printf(TEXT("AllianceOfficeReferenceName_%d"), Office.Id),
				20, FLinearColor(0.91f, 0.84f, 0.65f, 1.0f), true);
			Name->SetText(FormatName(Office));
			PlaceOnCanvas(ModeCanvas, Name, CapturePoint(302.0f, Y + 5.0f), FVector2D(880.0f, 30.0f), 6);
			UTextBlock* Description = MakeScreenText(*FString::Printf(TEXT("AllianceOfficeReferenceDescription_%d"), Office.Id),
				16, FLinearColor(0.84f, 0.84f, 0.81f, 1.0f), false);
			Description->SetText(FormatDescription(Office));
			PlaceOnCanvas(ModeCanvas, Description, CapturePoint(302.0f, Y + 39.0f), FVector2D(870.0f, 26.0f), 6);
			UTextBlock* Bonus = MakeScreenText(*FString::Printf(TEXT("AllianceOfficeReferenceBonus_%d"), Office.Id),
				15, FLinearColor(0.75f, 0.82f, 0.66f, 1.0f), false);
			Bonus->SetAutoWrapText(true);
			Bonus->SetText(FormatBonus(Office));
			PlaceOnCanvas(ModeCanvas, Bonus, CapturePoint(302.0f, Y + 66.0f), FVector2D(870.0f, 52.0f), 6);
			PlaceOnCanvas(ModeCanvas, MakeActionButton(
				*FString::Printf(TEXT("AllianceOfficeReferenceHit_%d"), Office.Id), 640 + Index,
				nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(1040.0f, 118.0f)),
				CapturePoint(198.0f, Y), FVector2D(1040.0f, 118.0f), 8);
		}
	}
	else
	{
		UTextBlock* Unavailable = MakeScreenText(TEXT("AllianceOfficeReferenceUnavailable"), 18,
			FLinearColor(0.82f, 0.80f, 0.76f, 1.0f), false, ETextJustify::Center);
		Unavailable->SetText(NSLOCTEXT("RokAlliance", "OfficeDefinitionsUnavailable",
			"官职静态配置暂不可用；未读取任命或权限状态"));
		PlaceOnCanvas(ModeCanvas, Unavailable, CapturePoint(250.0f, 440.0f), FVector2D(900.0f, 38.0f), 6);
	}
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceOfficeReferenceBack"), 634,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f),
		NSLOCTEXT("RokAlliance", "OfficeReferenceBack", "返回"), 18,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(208.0f, 735.0f), FVector2D(138.0f, 54.0f), 7);
}

#include "RokFeatureAllianceTechnology.inl"

void URokUnityFeatureScreenWidget::BuildAllianceCreate()
{
	FRokUnityAllianceCreateConfig AllianceConfig;
	if (URokUnityEconomyCatalogSubsystem* Catalog = GetEconomyCatalogSubsystem())
	{
		if (!Catalog->IsCatalogLoaded())
		{
			Catalog->LoadCatalog();
		}
		if (Catalog->IsCatalogLoaded())
		{
			AllianceConfig = Catalog->GetAllianceCreateConfig();
		}
	}
	static const FText InputHints[] = {
		NSLOCTEXT("RokAlliance", "AllianceNameHint", "请输入联盟名称"),
		NSLOCTEXT("RokAlliance", "AllianceShortNameHint", "请输入联盟简称"),
		NSLOCTEXT("RokAlliance", "AllianceNoticeHint", "请输入联盟公告"),
	};
	static const FName InputNames[] = {
		TEXT("AllianceCreateNameInput"),
		TEXT("AllianceCreateTagInput"),
		TEXT("AllianceCreateNoticeInput"),
	};
	const FString* DraftValues[] = {&AllianceCreateNameDraft, &AllianceCreateTagDraft, &AllianceCreateNoticeDraft};
	const auto FormatHint = [&AllianceConfig](int32 Index)
	{
		FText Hint = InputHints[Index];
		if (Index == 0
			&& AllianceConfig.NameMinCharacters > 0
			&& AllianceConfig.NameMaxCharacters >= AllianceConfig.NameMinCharacters)
		{
			Hint = FText::Format(
				NSLOCTEXT("RokAlliance", "AllianceNameRangeHint", "请输入联盟名称（{0}-{1}个字符）"),
				FText::AsNumber(AllianceConfig.NameMinCharacters),
				FText::AsNumber(AllianceConfig.NameMaxCharacters));
		}
		else if (Index == 1
			&& AllianceConfig.TagMinCharacters > 0
			&& AllianceConfig.TagMaxCharacters >= AllianceConfig.TagMinCharacters)
		{
			Hint = FText::Format(
				NSLOCTEXT("RokAlliance", "AllianceTagRangeHint", "请输入联盟简称（{0}-{1}个字符）"),
				FText::AsNumber(AllianceConfig.TagMinCharacters),
				FText::AsNumber(AllianceConfig.TagMaxCharacters));
		}
		else if (Index == 2 && AllianceConfig.NoticeMaxCharacters > 0)
		{
			Hint = FText::Format(
				NSLOCTEXT("RokAlliance", "AllianceNoticeLimitHint", "请输入联盟公告（最多{0}个字符）"),
				FText::AsNumber(AllianceConfig.NoticeMaxCharacters));
		}
		return Hint;
	};
	const RokAllianceFlagLocalPreview::FEntry* Cloth = RokAllianceFlagLocalPreview::Find(1, AllianceCreateFlagDraft.ClothId);
	const RokAllianceFlagLocalPreview::FEntry* ClothColor = RokAllianceFlagLocalPreview::Find(2, AllianceCreateFlagDraft.ClothColorId);
	const RokAllianceFlagLocalPreview::FEntry* Emblem = RokAllianceFlagLocalPreview::Find(3, AllianceCreateFlagDraft.EmblemId);
	const RokAllianceFlagLocalPreview::FEntry* EmblemColor = RokAllianceFlagLocalPreview::Find(4, AllianceCreateFlagDraft.EmblemColorId);
	const FText CostText = AllianceConfig.EstablishCost > 0
		? FText::Format(
			NSLOCTEXT("RokAlliance", "CreateCost", "创建消耗：{0}"),
			FText::AsNumber(AllianceConfig.EstablishCost))
		: NSLOCTEXT("RokAlliance", "CreateCostUnavailable", "创建消耗：--");

	if (const URokUiPageWidget* Page = MountModePage(TEXT("AllianceCreate")))
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(InputNames); ++Index)
		{
			if (UEditableTextBox* Input = Page->Get<UEditableTextBox>(InputNames[Index]))
			{
				Input->SetHintText(FormatHint(Index));
				Input->SetText(FText::FromString(*DraftValues[Index]));
			}
		}
		SetFeaturePageText(*Page, TEXT("AllianceCreateCost"), CostText);
		UTexture2D* FlagAtlas = LoadUiTexture(AllianceFlagAtlasPath);
		const auto UpdateFlagLayer = [this, Page, FlagAtlas](FName Name, const FIntRect& Rect, FLinearColor Tint)
		{
			if (UImage* Layer = Page->Get<UImage>(Name))
			{
				Page->SetShown(Layer, FlagAtlas != nullptr);
				if (FlagAtlas)
				{
					FSlateBrush Brush = Layer->GetBrush();
					const FSlateBrush AtlasBrush = MakeAtlasBrush(FlagAtlas, Rect, AllianceFlagAtlasSize, Brush.GetImageSize());
					Brush.SetResourceObject(FlagAtlas);
					Brush.SetUVRegion(static_cast<FBox2f>(AtlasBrush.GetUVRegion()));
					Layer->SetBrush(Brush);
				}
				Layer->SetColorAndOpacity(Tint);
			}
		};
		UpdateFlagLayer(TEXT("AllianceCreateFlagCloth"), Cloth ? Cloth->RealityRect : AllianceFlagClothRect,
			ClothColor ? RokAllianceFlagLocalPreview::LinearColor(*ClothColor) : FLinearColor(0.14f, 0.36f, 0.72f, 1.0f));
		UpdateFlagLayer(TEXT("AllianceCreateFlagEmblem"), Emblem ? Emblem->RealityRect : AllianceFlagEmblemRect,
			EmblemColor ? RokAllianceFlagLocalPreview::LinearColor(*EmblemColor) : FLinearColor(0.96f, 0.76f, 0.18f, 1.0f));
		BindPageAction(*Page, TEXT("AllianceFlagEdit"), 625);
		BindPageAction(*Page, TEXT("AllianceCreateConfirm"), 623);
		BindPageAction(*Page, TEXT("AllianceCreateBack"), 622);
		return;
	}

	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f), NSLOCTEXT("RokAlliance", "CreateAllianceTitle", "创建联盟"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceCreateShade"), FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)), CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);
	UTextBlock* Intro = MakeScreenText(TEXT("AllianceCreateIntro"), 22, FLinearColor(0.92f, 0.90f, 0.84f, 1.0f), true, ETextJustify::Center);
	Intro->SetText(NSLOCTEXT("RokAlliance", "UniqueAlliance", "每一个联盟，都独一无二。"));
	PlaceOnCanvas(ModeCanvas, Intro, CapturePoint(205.0f, 174.0f), FVector2D(600.0f, 38.0f), 5);

	static const FText FieldLabels[] = {
		NSLOCTEXT("RokAlliance", "AllianceNameField", "联盟名称"),
		NSLOCTEXT("RokAlliance", "AllianceShortNameField", "联盟简称"),
		NSLOCTEXT("RokAlliance", "AllianceNoticeField", "联盟公告"),
		NSLOCTEXT("RokAlliance", "JoinRequirementField", "入盟要求"),
		NSLOCTEXT("RokAlliance", "LanguageField", "语言"),
	};
	static const FText FieldValues[] = {
		FText::GetEmpty(),
		FText::GetEmpty(),
		FText::GetEmpty(),
		NSLOCTEXT("RokAlliance", "AnyoneCanJoin", "任何人均可加入"),
		NSLOCTEXT("RokAlliance", "ChineseLanguage", "中文"),
	};
	static const float FieldHeights[] = {52.0f, 52.0f, 106.0f, 52.0f, 52.0f};
	float FieldY = 228.0f;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FieldLabels); ++Index)
	{
		UTextBlock* Label = MakeScreenText(*FString::Printf(TEXT("AllianceCreateLabel_%d"), Index), 18, FLinearColor(0.86f, 0.84f, 0.79f, 1.0f), false);
		Label->SetText(FieldLabels[Index]);
		PlaceOnCanvas(ModeCanvas, Label, CapturePoint(208.0f, FieldY + 10.0f), FVector2D(130.0f, 32.0f), 5);
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(*FString::Printf(TEXT("AllianceCreateFieldBg_%d"), Index), FLinearColor(0.18f, 0.18f, 0.18f, 1.0f)), CapturePoint(342.0f, FieldY), FVector2D(455.0f, FieldHeights[Index]), 4);
		if (Index < UE_ARRAY_COUNT(InputNames))
		{
			UEditableTextBox* Input =
				WidgetTree->ConstructWidget<UEditableTextBox>(
					UEditableTextBox::StaticClass(),
					InputNames[Index]);
			Input->SetHintText(FormatHint(Index));
			FEditableTextBoxStyle InputStyle = FEditableTextBoxStyle::GetDefault();
			InputStyle
				.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18))
				.SetForegroundColor(FLinearColor::White)
				.SetFocusedForegroundColor(FLinearColor::White)
				.SetBackgroundColor(FLinearColor(0.18f, 0.18f, 0.18f, 1.0f))
				.SetPadding(FMargin(10.0f, 6.0f));
			Input->SetWidgetStyle(InputStyle);
			Input->SetClearKeyboardFocusOnCommit(false);
			Input->SetText(FText::FromString(*DraftValues[Index]));
			PlaceOnCanvas(
				ModeCanvas,
				Input,
				CapturePoint(350.0f, FieldY + 4.0f),
				FVector2D(440.0f, FieldHeights[Index] - 8.0f),
				5);
		}
		else
		{
			UTextBlock* Value = MakeScreenText(*FString::Printf(TEXT("AllianceCreateValue_%d"), Index), 18, FLinearColor(0.93f, 0.92f, 0.88f, 1.0f), false);
			Value->SetText(FieldValues[Index]);
			PlaceOnCanvas(ModeCanvas, Value, CapturePoint(360.0f, FieldY + 10.0f), FVector2D(420.0f, FieldHeights[Index] - 14.0f), 5);
		}
		FieldY += FieldHeights[Index] + 18.0f;
	}

	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceCreateFlagPanel"), FLinearColor(0.24f, 0.23f, 0.20f, 0.98f)), CapturePoint(835.0f, 188.0f), FVector2D(370.0f, 430.0f), 4);
	UTextBlock* FlagTitle = MakeScreenText(TEXT("AllianceCreateFlagTitle"), 21, FLinearColor(0.92f, 0.90f, 0.82f, 1.0f), true, ETextJustify::Center);
	FlagTitle->SetText(NSLOCTEXT("RokAlliance", "AllianceFlag", "联盟旗帜"));
	PlaceOnCanvas(ModeCanvas, FlagTitle, CapturePoint(865.0f, 205.0f), FVector2D(310.0f, 36.0f), 6);
	UImage* FlagCloth = MakeAtlasImage(TEXT("AllianceCreateFlagCloth"), AllianceFlagAtlasPath, Cloth ? Cloth->RealityRect : AllianceFlagClothRect, AllianceFlagAtlasSize, FVector2D(184.0f, 216.0f));
	FlagCloth->SetColorAndOpacity(ClothColor ? RokAllianceFlagLocalPreview::LinearColor(*ClothColor) : FLinearColor(0.14f, 0.36f, 0.72f, 1.0f));
	PlaceOnCanvas(ModeCanvas, FlagCloth, CapturePoint(925.0f, 258.0f), FVector2D(184.0f, 216.0f), 6);
	UImage* FlagEmblem = MakeAtlasImage(TEXT("AllianceCreateFlagEmblem"), AllianceFlagAtlasPath, Emblem ? Emblem->RealityRect : AllianceFlagEmblemRect, AllianceFlagAtlasSize, FVector2D(100.0f));
	FlagEmblem->SetColorAndOpacity(EmblemColor ? RokAllianceFlagLocalPreview::LinearColor(*EmblemColor) : FLinearColor(0.96f, 0.76f, 0.18f, 1.0f));
	PlaceOnCanvas(ModeCanvas, FlagEmblem, CapturePoint(967.0f, 310.0f), FVector2D(100.0f), 7);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceFlagEdit"), 625, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f), NSLOCTEXT("RokAlliance", "ChooseFlag", "选择旗帜"), 17, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(950.0f, 510.0f), FVector2D(138.0f, 54.0f), 8);

	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceCreateGem"), CommonAtlasPath, GemRect, CommonAtlasSize, FVector2D(42.0f)), CapturePoint(854.0f, 644.0f), FVector2D(42.0f), 6);
	UTextBlock* Cost = MakeScreenText(TEXT("AllianceCreateCost"), 19, FLinearColor(0.96f, 0.88f, 0.55f, 1.0f), true);
	Cost->SetText(CostText);
	PlaceOnCanvas(ModeCanvas, Cost, CapturePoint(900.0f, 650.0f), FVector2D(210.0f, 32.0f), 6);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceCreateConfirm"), 623, CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize, FVector2D(202.0f, 81.0f), NSLOCTEXT("RokAlliance", "CreateNow", "创建"), 27, FLinearColor(0.23f, 0.16f, 0.04f, 1.0f)), CapturePoint(920.0f, 700.0f), FVector2D(202.0f, 81.0f), 8);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceCreateBack"), 622, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f), NSLOCTEXT("RokAlliance", "BackFromCreate", "返回"), 18, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(208.0f, 735.0f), FVector2D(138.0f, 54.0f), 8);
}

namespace
{
int32 GetAllianceFlagEditorPreviewTab()
{
#if !UE_BUILD_SHIPPING
	int32 Type = 0;
	if (FParse::Param(FCommandLine::Get(), TEXT("RokLocalOnly"))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokAllianceCreatePreview"))
		&& FParse::Value(FCommandLine::Get(), TEXT("RokAllianceFlagEditorPreview="), Type)
		&& Type >= 1 && Type <= 4)
	{
		return Type;
	}
#endif
	return INDEX_NONE;
}

void ClickAllianceFlagEditorPreviewButton(const URokUnityFeatureScreenWidget& Screen, FName Name)
{
	if (UButton* Button = Cast<UButton>(Screen.GetWidgetFromName(Name)); Button && Button->GetIsEnabled())
	{
		Button->OnClicked.Broadcast();
		UE_LOG(LogTemp, Display, TEXT("Rok alliance flag preview: clicked %s."), *Name.ToString());
	}
}
}

#include "RokFeatureAllianceFlagEditor.inl"

#include "RokFeatureAllianceJoined.inl"

#include "RokFeatureCommanderHead.inl"

UCanvasPanel* URokUnityFeatureScreenWidget::MakeRewardCell(
	const FName& Name,
	const TCHAR* TexturePath,
	const FIntRect& IconRect,
	const FIntPoint& AtlasSize,
	const FText& CountText,
	const FText& TopText,
	int32 QualityStyle,
	const FVector2D& DesiredSize)
{
	UCanvasPanel* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
	const FIntRect QualityRect = QualityStyle == 2 ? ItemQualityLegendaryRect : (QualityStyle == 1 ? ItemQualityEpicRect : ItemQualityBlueRect);
	PlaceOnCanvas(Cell, MakeAtlasImage(*FString::Printf(TEXT("%sQuality"), *Name.ToString()), Item0AtlasPath, QualityRect, Item0AtlasSize, DesiredSize), FVector2D::ZeroVector, DesiredSize);
	const float Inset = DesiredSize.X * 0.045f;
	PlaceOnCanvas(Cell, MakeAtlasImage(*FString::Printf(TEXT("%sIcon"), *Name.ToString()), TexturePath, IconRect, AtlasSize, DesiredSize - FVector2D(Inset * 2.0f)), FVector2D(Inset), DesiredSize - FVector2D(Inset * 2.0f), 2);
	if (!TopText.IsEmpty())
	{
		const FVector2D TopSize(DesiredSize.X - 6.0f, DesiredSize.Y * 0.22f);
		PlaceOnCanvas(Cell, MakeAtlasImage(*FString::Printf(TEXT("%sTop"), *Name.ToString()), CommonAtlasPath, ItemTopRect, CommonAtlasSize, TopSize), FVector2D(3.0f, 2.0f), TopSize, 3);
		UTextBlock* Top = MakeScreenText(*FString::Printf(TEXT("%sTopText"), *Name.ToString()), 12, FLinearColor::White, false, ETextJustify::Center);
		Top->SetText(TopText);
		PlaceOnCanvas(Cell, Top, FVector2D(3.0f, 1.0f), FVector2D(DesiredSize.X - 6.0f, 19.0f), 4);
	}
	UTextBlock* Count = MakeScreenText(*FString::Printf(TEXT("%sCount"), *Name.ToString()), 15, FLinearColor::White, true, ETextJustify::Right);
	Count->SetText(CountText);
	PlaceOnCanvas(Cell, Count, FVector2D(DesiredSize.X * 0.5f, DesiredSize.Y - 25.0f), FVector2D(DesiredSize.X * 0.44f, 22.0f), 4);
	return Cell;
}

void URokUnityFeatureScreenWidget::BuildFeedback()
{
	if (ActiveModePage)
	{
		FeedbackText = Cast<UTextBlock>(RokUiFind::Find(ActiveModePage, TEXT("FeatureBoundaryFeedback")));
		if (FeedbackText)
		{
			return;
		}
	}

	FVector2D FeedbackPosition(360.0f, 778.0f);
	FVector2D FeedbackSize(720.0f, 28.0f);
	FName LayoutName;
	if (ActiveMode == ERokUnityFeatureScreenMode::Chat)
	{
		FeedbackPosition = FVector2D(350.0f, bChatEmojiPanelOpen ? 432.0f : 692.0f);
		FeedbackSize = FVector2D(645.0f, 28.0f);
		LayoutName = bChatEmojiPanelOpen ? TEXT("ChatEmojiLayout") : TEXT("ChatLayout");
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::Commander)
	{
		FeedbackPosition.Y = 728.0f;
		LayoutName = TEXT("CommanderLayout");
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::Expedition)
	{
		FeedbackPosition.Y = 752.0f;
		LayoutName = TEXT("ExpeditionLayout");
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::WorldPositionSearch)
	{
		// The source uses TipStyle.Middle for its empty-input message.
		FeedbackPosition = FVector2D(560.0f, 385.0f);
		FeedbackSize = FVector2D(320.0f, 40.0f);
		LayoutName = TEXT("WorldPositionSearchLayout");
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::ResourceSearch)
	{
		FeedbackPosition = FVector2D(420, 332);
		FeedbackSize = FVector2D(600, 40);
		LayoutName = TEXT("ResourceSearchLayout");
	}

	UCanvasPanel* Parent = ChatContentCanvas ? ChatContentCanvas.Get() : ModeCanvas.Get();
	UClass* FeedbackClass = ResolveFeaturePageClass(TEXT("FeatureFeedback"));
	if (auto* Page = FeedbackClass ? CreateWidget<URokUiPageWidget>(this, FeedbackClass) : nullptr)
	{
		auto* Label = Page->Get<UTextBlock>(TEXT("FeatureBoundaryFeedback"));
		auto* LabelSlot = Label ? Cast<UCanvasPanelSlot>(Label->Slot) : nullptr;
		const auto* Layout = LayoutName.IsNone() ? nullptr : Page->Get<UCanvasPanel>(LayoutName);
		const auto* LayoutSlot = Layout ? Cast<UCanvasPanelSlot>(Layout->Slot) : nullptr;
		if (LabelSlot && (LayoutName.IsNone() || LayoutSlot))
		{
			if (LayoutSlot)
			{
				LabelSlot->SetLayout(LayoutSlot->GetLayout());
			}
			Page->SetVisibility(ESlateVisibility::HitTestInvisible);
			auto* PageSlot = Parent->AddChildToCanvas(Page);
			PageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
			PageSlot->SetOffsets(FMargin(0));
			PageSlot->SetZOrder(50);
			FeedbackText = Label;
		}
	}
	if (!FeedbackText)
	{
		FeedbackText = MakeScreenText(TEXT("FeatureBoundaryFeedback"), 16,
			FLinearColor(1.0f, 0.87f, 0.45f, 1.0f), true, ETextJustify::Center);
		PlaceOnCanvas(Parent, FeedbackText, FeedbackPosition, FeedbackSize, 50);
	}
	FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	if (ActiveMode == ERokUnityFeatureScreenMode::WorldPositionSearch && !WorldPositionMiddleTipRoot)
	{
		// UI_Common_MidTips: centered 640x60 text; tips_com_mid is bottom-left (1,615,601,69).
		WorldPositionMiddleTipRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("WorldPositionMiddleTipRoot"));
		PlaceOnCanvas(ModeCanvas, WorldPositionMiddleTipRoot, FVector2D::ZeroVector,
			FVector2D(640.0f, 69.0f), 60);
		if (UCanvasPanelSlot* TipRootSlot = Cast<UCanvasPanelSlot>(WorldPositionMiddleTipRoot->Slot))
		{
			TipRootSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			TipRootSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			TipRootSlot->SetPosition(FVector2D::ZeroVector);
		}
		WorldPositionMiddleTipRoot->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		WorldPositionMiddleTipBackground = MakeAtlasImage(
			TEXT("WorldPositionMiddleTipBackground"), CommonAtlasPath,
			SpriteRect(1, 615, 601, 69), CommonAtlasSize, FVector2D(601.0f, 69.0f));
		WorldPositionMiddleTipBackground->SetColorAndOpacity(
			FLinearColor(1.0f, 1.0f, 1.0f, 0.60784316f));
		WorldPositionMiddleTipBackground->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		PlaceOnCanvas(WorldPositionMiddleTipRoot, WorldPositionMiddleTipBackground,
			FVector2D(19.5f, 0.0f), FVector2D(601.0f, 69.0f));
		// Source LanguageText uses 22 pixels. SlateFontInfo.Size is points rendered at 96 DPI.
		// Keep this conversion local to the Middle tip; other feature text uses its own sizes.
		WorldPositionMiddleTipText = MakeScreenText(
			TEXT("WorldPositionMiddleTipText"), 22, FLinearColor::White,
			false, ETextJustify::Center);
		FSlateFontInfo TipFont = WorldPositionMiddleTipText->GetFont();
		TipFont.Size = 22.0f * 72.0f / static_cast<float>(FontConstants::RenderDPI);
		WorldPositionMiddleTipText->SetFont(TipFont);
		WorldPositionMiddleTipText->SetShadowOffset(FVector2D(1.0f, 1.0f));
		WorldPositionMiddleTipText->SetShadowColorAndOpacity(
			FLinearColor(0.0f, 0.0f, 0.0f, 0.5f));
		// Source lbl_message is middle-center in its 640x60 rect. UTextBlock alone
		// top-aligns inside a fixed CanvasSlot, so give it a centered content host.
		UBorder* TipTextHost = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("WorldPositionMiddleTipTextHost"));
		TipTextHost->SetPadding(FMargin(0.0f));
		TipTextHost->SetBrushColor(FLinearColor::Transparent);
		TipTextHost->SetHorizontalAlignment(HAlign_Center);
		TipTextHost->SetVerticalAlignment(VAlign_Center);
		TipTextHost->SetContent(WorldPositionMiddleTipText);
		PlaceOnCanvas(WorldPositionMiddleTipRoot, TipTextHost,
			FVector2D(0.0f, 4.5f), FVector2D(640.0f, 60.0f), 1);
		WorldPositionMiddleTipRoot->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URokUnityFeatureScreenWidget::UpdateExpandedDesignResolution(const FVector2D& RootSize)
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

void URokUnityFeatureScreenWidget::UpdateActivityCountdown(float DeltaSeconds)
{
	if (!bFreezeActivityCountdownForCapture)
	{
		ActivityPreviewSecondsRemaining = FMath::Max(0.0, ActivityPreviewSecondsRemaining - static_cast<double>(DeltaSeconds));
	}
	const int32 TotalSeconds = FMath::FloorToInt(ActivityPreviewSecondsRemaining);
	if (!ActivityCountdownText || TotalSeconds == LastRenderedActivitySecond)
	{
		return;
	}
	LastRenderedActivitySecond = TotalSeconds;
	const int32 Days = TotalSeconds / 86400;
	const int32 Hours = (TotalSeconds / 3600) % 24;
	const int32 Minutes = (TotalSeconds / 60) % 60;
	const int32 Seconds = TotalSeconds % 60;
	ActivityCountdownText->SetText(FText::FromString(FString::Printf(TEXT("开始倒计时：%d天 %02d:%02d:%02d"), Days, Hours, Minutes, Seconds)));
}

void URokUnityFeatureScreenWidget::HandleIndexedAction(int32 ActionId)
{
	// The source's force-close button bypasses IsAniCloseing, including while
	// the ResearchMain mask-initiated Close clip is in progress.
	if (ActiveMode == ERokUnityFeatureScreenMode::Research &&
		bResearchMainMaskClosing && ActionId == 1)
	{
		PendingActionId = ActionId;
		return;
	}
	if (WindowAnimation.IsClosing() || ChatDrawerAnimation.IsClosing() || ResourceSearchAnimation.IsClosing())
	{
		return;
	}
	// Rebuilding the WidgetTree while the clicked button is still broadcasting
	// can replace that same named UObject in-place. Defer every child action to
	// the next widget tick so the Slate/UMG callback stack has unwound first.
	PendingActionId = ActionId;
}

void URokUnityFeatureScreenWidget::ExecuteIndexedAction(int32 ActionId)
{
	// A pending root Close owns the transition. Direct or future dispatches
	// cannot execute a second action before the original is released.
	if (PendingAllianceRootCloseActionId != INDEX_NONE
		&& !bDispatchingAllianceRootCloseAction) return;
	if (!bDispatchingAllianceRootCloseAction
		&& TryStartAllianceRootClose(ActionId)) return;
	if (ActiveMode == ERokUnityFeatureScreenMode::Alliance && bAllianceRankingReferenceOpen)
	{
		if (ActionId == 1 || ActionId == 19710)
		{
			if (SelectedAllianceRankingCategoryId != INDEX_NONE)
			{
				SelectedAllianceRankingCategoryId = INDEX_NONE;
				RebuildModeContent();
			}
			else if (ActionId == 19710)
			{
				bAllianceRankingReferenceOpen = false;
				RebuildModeContent();
			}
			else
			{
				RequestCloseScreen();
			}
			return;
		}
		if (SelectedAllianceRankingCategoryId == INDEX_NONE
			&& ActionId >= 19700 && ActionId < 19706)
		{
			TArray<FRokLeaderboardRow> Categories;
			FString LoadError;
			if (RokAllianceRankingReadonlyCatalog::Load(Categories, LoadError)
				&& Categories.IsValidIndex(ActionId - 19700))
			{
				SelectedAllianceRankingCategoryId = Categories[ActionId - 19700].Id;
				RebuildModeContent();
			}
			return;
		}
		return;
	}
	if (ActionId == 1)
	{
		RequestCloseScreen();
		return;
	}
	// While the reference page is open, only local navigation is accepted.
	if (ActiveMode == ERokUnityFeatureScreenMode::Alliance && bAllianceTerritoryReferenceOpen)
	{
		if (ActionId == 695)
		{
			bAllianceTerritoryReferenceOpen = false;
			ExpandedAllianceTerritoryGroup = INDEX_NONE;
			SelectedAllianceTerritoryType = 0;
			RebuildModeContent();
			return;
		}
		if (ActionId == 19630)
		{
			SelectedAllianceTerritoryType = 0;
			RebuildModeContent();
			return;
		}
		if (ActionId >= 19620 && ActionId <= 19623)
		{
			const int32 Group = ActionId - 19620;
			ExpandedAllianceTerritoryGroup = ExpandedAllianceTerritoryGroup == Group ? INDEX_NONE : Group;
			SelectedAllianceTerritoryType = 0;
			RebuildModeContent();
			return;
		}
		if (ActionId >= 19601 && ActionId <= 19612)
		{
			const int32 Type = ActionId - 19600;
			const RokAllianceTerritoryReadonly::FRow* Row = RokAllianceTerritoryReadonly::FindRow(Type);
			if (Row && Row->GroupIndex == ExpandedAllianceTerritoryGroup)
			{
				SelectedAllianceTerritoryType = Type;
				RebuildModeContent();
			}
		}
		return;
	}
	if (ActionId >= 19000 && ActionId < 19500)
	{
		if (ActiveMode != ERokUnityFeatureScreenMode::Alliance || AlliancePreviewState != 3
			|| !bAllianceFlagEditorOpen) return;
		if (ActionId >= 19001 && ActionId <= 19004)
		{
			SelectedAllianceFlagType = ActionId - 19000;
			RebuildModeContent();
			return;
		}
		if (ActionId == 19010)
		{
			bAllianceFlagEditorOpen = false;
			AllianceFlagEditDraft = AllianceCreateFlagDraft;
			RebuildModeContent();
			return;
		}
		if (ActionId == 19011)
		{
			if (!RokAllianceFlagLocalPreview::IsValidDraft(AllianceFlagEditDraft)) return;
			AllianceCreateFlagDraft = AllianceFlagEditDraft;
			bAllianceFlagEditorOpen = false;
			RebuildModeContent();
			return;
		}
		const int32 Type = (ActionId - 19000) / 100;
		const int32 RequestedIndex = (ActionId - 19000) % 100;
		if (Type < 1 || Type > 4 || RequestedIndex < 0) return;
		int32 LocalIndex = 0;
		for (const RokAllianceFlagLocalPreview::FEntry& Entry : RokAllianceFlagLocalPreview::Rows())
		{
			if (Entry.Type != Type) continue;
			if (LocalIndex++ != RequestedIndex) continue;
			AllianceFlagEditDraft.SetId(Type, Entry.Id);
			RebuildModeContent();
			return;
		}
		return;
	}
	if (ActionId >= 16000 && ActionId < 16089)
	{
		if (ActiveMode != ERokUnityFeatureScreenMode::ActivityCenter || SelectedActivityIndex != 0) return;
		URokUnityGameplayCatalogSubsystem* Catalog = GetGameInstance()
			? GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
		if (!Catalog || !Catalog->LoadActivityCalendarReadonly()) return;
		const TArray<FRokUnityActivityCalendarReadonlyRow>& Rows = Catalog->GetActivityCalendarReadonlyRows();
		const int32 Index = ActionId - 16000;
		if (!Rows.IsValidIndex(Index)) return;
		if (UScrollBox* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ActivityCalendarReferenceScroll"))))
		{
			ActivityCalendarScrollOffset = Scroll->GetScrollOffset();
		}
		SelectedActivityCalendarId = Rows[Index].Id;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 100 && ActionId < 105)
	{
		const int32 RequestedIndex = ActionId - 100;
		if (RequestedIndex == 0)
		{
			URokUnityGameplayCatalogSubsystem* Catalog = GetGameInstance()
				? GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
			if (!Catalog || !Catalog->LoadActivityCalendarReadonly())
			{
				ShowBoundaryFeedback(NSLOCTEXT("RokFeature", "ActivityCalendarConfigMissing",
					"活动配置暂不可用"));
				return;
			}
			SelectedActivityIndex = 0;
			SelectedActivityCalendarId = 0;
			ActivityCalendarScrollOffset = 0.0f;
			RebuildModeContent();
			const int32 PreviewEntry = ReadLocalUiPreviewOption(TEXT("RokActivityCalendarEntry="), 1, MAX_int32);
			if (PreviewEntry != INDEX_NONE)
			{
				const FName Name(*FString::Printf(TEXT("ActivityCalendarReferenceHit_%d"), PreviewEntry));
				if (UButton* Button = Cast<UButton>(GetWidgetFromName(Name)); Button && Button->GetIsEnabled())
				{
					Button->OnClicked.Broadcast();
					UE_LOG(LogTemp, Display, TEXT("Rok activity preview: clicked %s."), *Name.ToString());
				}
			}
		}
		else if (RequestedIndex == 1 || RequestedIndex == 2)
		{
			SelectedActivityIndex = RequestedIndex;
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok activity tab switch: tab=%d local_ui_state=1."),
				SelectedActivityIndex);
			RebuildModeContent();
		}
		else
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok activity tab boundary: tab=%d state_unchanged=1 authority=server."),
				RequestedIndex);
			ShowBoundaryFeedback(NSLOCTEXT("RokFeature", "ActivityPreviewBoundary", "该活动详情尚未迁入本地预览，当前保持城主之战。"));
		}
		return;
	}
	if (ActionId == 190)
	{
		ShowBoundaryFeedback(NSLOCTEXT("RokFeature", "RankBoundary", "历史排行服务尚未接入，当前仅显示 Unity 来源预览。"));
		return;
	}
	if (ActionId >= 200 && ActionId < 206)
	{
		SelectedMailTab = ActionId - 200;
		SelectedMailIndex = bLocalMailUnreadFixtureState
			? MailLocalUnreadState.SelectFirstAndMarkReadForTab(SelectedMailTab) : 0;
		ActiveMailRewardTooltipIndex = INDEX_NONE;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 220 && ActionId < (bLocalMailUnreadFixtureState ? 220 + MailLocalUnreadState.Rows.Num() : 226))
	{
		if (bLocalMailUnreadFixtureState)
		{
			const auto* Row = MailLocalUnreadState.Find(ActionId - 220);
			if (!Row || Row->Tab != SelectedMailTab) return;
			MailLocalUnreadState.MarkRead(ActionId - 220, SelectedMailTab);
		}
		SelectedMailIndex = ActionId - 220;
		ActiveMailRewardTooltipIndex = INDEX_NONE;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 2600 && ActionId < 2600 + RenderedMailRewardRows.Num())
	{
		ShowMailRewardTooltip(ActionId - 2600);
		return;
	}
	if (ActionId == -2698)
	{
		// Unity HelpTipMediator ignores pointer hits inside the tip subtree.
		return;
	}
	if (ActionId == 240 || ActionId == 241 || (ActionId >= 250 && ActionId < 254))
	{
		LastMailBoundaryActionId = ActionId;
		ShowBoundaryFeedback(NSLOCTEXT("RokFeature", "MailBoundary", "邮件服务尚未接入，未改变邮件或奖励状态。"));
		return;
	}
	if (ActionId >= 300 && ActionId < 303)
	{
		SelectedTaskPage = ActionId - 300;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 340 && ActionId < 347
		&& ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 0)
	{
		const int32 GroupId = ActionId - 339;
		FRokTaskChapterPreviewRow Chapter;
		if (RokTaskChapterConfigPreviewData::FindChapterById(TaskChapterGroups, GroupId, Chapter))
		{
			SelectedTaskChapterGroupId = GroupId;
			RebuildModeContent();
		}
		return;
	}
	if (ActionId == 319 && ActiveMode == ERokUnityFeatureScreenMode::Tasks
		&& SelectedTaskPage == 1 && bTaskTestFixtureState
		&& VisibleTaskStates.IsValidIndex(0)
		&& VisibleTaskStates[0].Page == FRokTaskRowState::EPage::MainTask)
	{
		UCanvasPanel* Row = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("TaskRow_0")));
		BeginTaskCompletedRowAnimation(Row, [this]()
		{
			++TaskCompletedAnimationFixtureCallbacks;
			UE_LOG(LogTemp, Display, TEXT("Rok task completed-row visual fixture callback: no request, no task/reward mutation."));
		});
		return;
	}
	if (ActionId == 6100 && ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 0)
	{
		const auto* Tasks = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerTaskSubsystem>() : nullptr;
		if (Tasks) SubmitTaskClaim(ERokTaskClaim::Chapter, Tasks->GetSnapshot().ChapterId);
		return;
	}
	if (ActionId >= 17800 && ActionId < 17805 && ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 2)
	{
		const auto* Tasks = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokServerTaskSubsystem>() : nullptr;
		const auto Snapshot = Tasks ? Tasks->GetSnapshot() : FRokServerTaskSnapshot();
		if (Snapshot.DailyMilestones.IsValidIndex(ActionId - 17800))
			SubmitTaskClaim(ERokTaskClaim::ActivePoints, Snapshot.DailyMilestones[ActionId - 17800].Points);
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Tasks && ((ActionId >= 320 && ActionId < 330)
		|| (ActionId >= 6000 && ActionId < 6000 + VisibleTaskStates.Num())))
	{
		const int32 RowIndex = ActionId >= 6000 ? ActionId - 6000 : ActionId - 320;
		if (VisibleTaskStates.IsValidIndex(RowIndex))
		{
			const FRokTaskRowState& RowState = VisibleTaskStates[RowIndex];
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok task action: task=%d page=%s state=%s authority=%s intent=%s."),
				RowState.TaskId,
				TaskRowPageToken(RowState.Page),
				TaskRowStateToken(RowState.State),
				*RowState.SourceAuthority.ToString(),
				TaskRowIntentToken(RowState.Intent));
			if (RowState.SourceAuthority == TEXT("server"))
			{
				if (RowState.Intent == FRokTaskRowState::EIntent::Claim)
				{
					ERokTaskClaim Kind = ERokTaskClaim::Main;
					switch (RowState.Page)
					{
					case FRokTaskRowState::EPage::SideTask: Kind = ERokTaskClaim::Side; break;
					case FRokTaskRowState::EPage::DailyTask: Kind = ERokTaskClaim::Daily; break;
					case FRokTaskRowState::EPage::ChapterTask: Kind = ERokTaskClaim::ChapterTask; break;
					default: break;
					}
					SubmitTaskClaim(Kind, RowState.TaskId);
				}
				else if (RowState.Intent == FRokTaskRowState::EIntent::Go)
				{
					APlayerController* Controller = GetOwningPlayer();
					auto* WorldController = Cast<ARokWorldMapPlayerController>(Controller);
					// TaskScriptGlobalMediator.GoScript routes RebelsNemesis to world search.
					if (RowState.Type == 1 && RowState.Param1 >= 1 && RowState.Param1 <= 25)
					{
						if (WorldController)
						{
							OpenResourceSearch(RowState.Param1);
							return;
						}
						if (auto* Pawn = Controller ? Cast<ARokStrategyCameraPawn>(Controller->GetPawn()) : nullptr)
						{
							if (Pawn->RequestWorldBarbarianSearch(RowState.Param1)) return;
						}
					}
					// BuildSomething uses the build menu, even when the same type has a city action.
					const bool bBuild = RowState.Type == 13 && RowState.Param1 > 0;
					const bool bUpgrade = RowState.Type == 8 && RowState.Param1 > 0;
					const bool bTraining = (RowState.Type == 10 || RowState.Type == 33)
						&& RowState.Param1 >= 1 && RowState.Param1 <= 4;
					const int32 BuildingType = (bBuild || bUpgrade) ? RowState.Param1
						: RowState.Type == 34 ? 13 : RowState.Type == 30 ? 14 : bTraining ? RowState.Param1 + 8 : 0;
					if (BuildingType > 0)
					{
						if (!WorldController)
						{
							OpenTaskCityTarget(BuildingType, bBuild, bUpgrade);
							return;
						}
						TActorIterator<ARokWorldMapConfigActor> Config(GetWorld());
						if (Config && WorldController->RequestEnterPlayerCity(
							Config->PlayerCityLevel, Config->GetCityEntryTargetDxf(), BuildingType, bBuild, bUpgrade)) return;
					}
					ShowBoundaryFeedback(NSLOCTEXT("RokFeature", "TaskServerGoUnavailable", "当前任务没有可打开的目标。"));
				}
				else if (RowState.State == FRokTaskRowState::EState::Received)
					ShowBoundaryFeedback(NSLOCTEXT("RokFeature", "TaskRewardAlreadyReceived", "该任务奖励已领取。"));
				else ShowBoundaryFeedback(NSLOCTEXT("RokFeature", "TaskServerGoUnavailable", "当前任务没有可打开的目标。"));
				return;
			}
			if (ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 1
				&& !bTaskTestFixtureState && RowIndex == 0
				&& RowState.Intent == FRokTaskRowState::EIntent::Go)
			{
				URokUnityRuntimeStateSubsystem* Runtime = GetRuntimeStateSubsystem();
				AActor* Hall = nullptr;
				FName Failure;
				if (Runtime && TryResolveLocalTask200900GoTarget(
					RowState, Runtime->GetActiveTask(), Runtime->GetPlayerProfileState(),
					Runtime->IsRuntimeStateLoaded(), GetWorld(), Hall, Failure)
					&& OnTaskGoTownHallRequested.IsBound()
					&& OnTaskGoTownHallRequested.Execute(Hall))
				{
					UE_LOG(LogTemp, Display, TEXT("Rok local task Go: task=200900 target=%s info_only=1."), *GetNameSafe(Hall));
					return;
				}
				UE_LOG(LogTemp, Warning, TEXT("Rok local task Go blocked: task=200900 reason=%s."),
					Runtime ? (Failure.IsNone() ? TEXT("hud_unavailable") : *Failure.ToString()) : TEXT("runtime_unavailable"));
			}
			ShowBoundaryFeedback(
				RowState.Intent == FRokTaskRowState::EIntent::Claim
					? NSLOCTEXT("RokFeature", "TaskClaimUnavailable", "奖励领取功能尚未开放，任务状态未改变。")
					: NSLOCTEXT("RokFeature", "TaskGoUnavailable", "无法定位该任务目标，任务状态未改变。"));
		}
		return;
	}
	if (ActionId == 330)
	{
		SelectedTaskPage = 1;
		RebuildModeContent();
		return;
	}
	if (ActionId == 499 && ActiveMode == ERokUnityFeatureScreenMode::Commander)
	{
		if (CommanderAnimatedPortrait) CommanderAnimatedPortrait->PlayStandby();
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Commander
		&& ActionId >= 400 && ActionId < 400 + VisibleCommanderIds.Num())
	{
		const int32 RequestedIndex = ActionId - 400;
		if (VisibleCommanderIds.IsValidIndex(RequestedIndex))
		{
			// Unity OnHeroSelected does nothing for the already selected hero.
			if (VisibleCommanderIds[RequestedIndex] == SelectedCommanderId) return;
			const int32 PreviousIndex = VisibleCommanderIds.IndexOfByKey(SelectedCommanderId);
			SelectedCommanderId = VisibleCommanderIds[RequestedIndex];
			bSelectedCommanderServerOwned = false; // Explicit unowned selection remains inspectable.
			bCommanderTalentOpen = false;
			bCommanderBiographyOpen = false;
			CommanderConfirmation.Reset();
			SelectedCommanderSkillIndex = 0;
			SelectedCommanderSkillDetailIndex = INDEX_NONE;
			SelectedCommanderTalentIndex = INDEX_NONE;
			bCommanderSortMenuOpen = false;
			const auto* OldRoster = Cast<UScrollBox>(GetWidgetFromName(TEXT("CommanderRosterScroll")));
			const float RosterOffset = OldRoster ? OldRoster->GetScrollOffset() : 0.0f;
			RebuildModeContent();
			if (auto* Roster = Cast<UScrollBox>(GetWidgetFromName(TEXT("CommanderRosterScroll"))))
				Roster->SetScrollOffset(RosterOffset);
			if (CommanderAnimatedPortrait)
				CommanderAnimatedPortrait->BeginSelectionTransition(RequestedIndex > PreviousIndex);
			UE_LOG(LogTemp, Log,
				TEXT("Rok commander select: hero=%d state_unchanged=1 authority=runtime_preview."),
				SelectedCommanderId);
		}
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Commander && HandleCommanderProgressionAction(ActionId)) return;
	if (ActionId == 468 && ActiveMode == ERokUnityFeatureScreenMode::Commander)
	{
		bCommanderBiographyOpen = !bCommanderBiographyOpen;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 490 && ActionId <= 492
		&& ActiveMode == ERokUnityFeatureScreenMode::Commander)
	{
		const int32 RequestedIndex = ActionId - 490;
		const bool bExplicitBootstrapHero = SelectedCommanderId == 1006
			|| SelectedCommanderId == 1007 || SelectedCommanderId == 1009
			|| SelectedCommanderId == 1010 || SelectedCommanderId == 1012;
		URokUnityRuntimeStateSubsystem* RuntimeState = GetRuntimeStateSubsystem();
		const bool bInLocalPreview = RuntimeState
			&& RuntimeState->GetStarterHeroes().ContainsByPredicate(
				[this](const FRokUnityRuntimeHeroState& Hero)
				{ return Hero.HeroId == SelectedCommanderId; });
		const UGameInstance* Instance = GetGameInstance();
		const URokServerHeroSubsystem* Server = Instance
			? Instance->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
		const FRokServerHeroSnapshot Snapshot = Server
			? Server->GetSnapshot() : FRokServerHeroSnapshot();
		const bool bServerOwned = Snapshot.bAvailable && Snapshot.Heroes.ContainsByPredicate(
			[this](const FRokServerHeroEntry& Hero) { return Hero.HeroId == SelectedCommanderId; });
		URokUnityGameplayCatalogSubsystem* GameplayCatalog = GetGameplayCatalogSubsystem();
		FRokUnityCatalogHeroTalent Talent;
		const int32 TalentId = VisibleCommanderTalentIds.IsValidIndex(RequestedIndex)
			? VisibleCommanderTalentIds[RequestedIndex] : INDEX_NONE;
		if (!(Snapshot.bAvailable ? bServerOwned : bExplicitBootstrapHero && bInLocalPreview)
			|| !GameplayCatalog
			|| TalentId <= 0 || !GameplayCatalog->GetHeroTalent(TalentId, Talent)
			|| (Talent.DisplayNameZhCn.IsEmpty() && Talent.DisplayNameEn.IsEmpty())
			|| (Talent.DisplayTipsZhCn.IsEmpty() && Talent.DisplayTipsEn.IsEmpty()))
		{
			ShowBoundaryFeedback(NSLOCTEXT("RokCommander", "TalentDetailUnavailable",
				"此统帅的天赋配置详情暂不可用。"));
			return;
		}
		SelectedCommanderTalentIndex = RequestedIndex;
		RebuildModeContent();
		UE_LOG(LogTemp, Log,
			TEXT("Rok commander talent config detail: hero=%d talent=%d local_preview=1 state_unchanged=1."),
			SelectedCommanderId, TalentId);
		return;
	}
	if (ActionId == 493 && ActiveMode == ERokUnityFeatureScreenMode::Commander)
	{
		SelectedCommanderTalentIndex = INDEX_NONE;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 460 && ActionId < 465
		&& ActiveMode == ERokUnityFeatureScreenMode::Commander)
	{
		const int32 RequestedIndex = ActionId - 460;
		const bool bExplicitBootstrapHero = SelectedCommanderId == 1006
			|| SelectedCommanderId == 1007 || SelectedCommanderId == 1009
			|| SelectedCommanderId == 1010 || SelectedCommanderId == 1012;
		URokUnityRuntimeStateSubsystem* RuntimeState = GetRuntimeStateSubsystem();
		const bool bInLocalPreview = RuntimeState
			&& RuntimeState->GetStarterHeroes().ContainsByPredicate(
				[this](const FRokUnityRuntimeHeroState& Hero)
				{ return Hero.HeroId == SelectedCommanderId; });
		const UGameInstance* Instance = GetGameInstance();
		const URokServerHeroSubsystem* Server = Instance
			? Instance->GetSubsystem<URokServerHeroSubsystem>() : nullptr;
		const FRokServerHeroSnapshot Snapshot = Server
			? Server->GetSnapshot() : FRokServerHeroSnapshot();
		URokUnityGameplayCatalogSubsystem* GameplayCatalog = GetGameplayCatalogSubsystem();
		FRokUnityCatalogHeroSkill Skill;
		FRokCommanderSkillReadonlyDetail Detail;
		const int32 SkillId = VisibleCommanderSkillIds.IsValidIndex(RequestedIndex)
			? VisibleCommanderSkillIds[RequestedIndex] : INDEX_NONE;
		if ((!Snapshot.bAvailable && bCommanderLocalRosterFixture && !(bExplicitBootstrapHero && bInLocalPreview))
			|| !GameplayCatalog
			|| SkillId <= 0 || !GameplayCatalog->GetHeroSkill(SkillId, Skill)
			|| !RokCommanderSkillReadonlyDetail::Build(Skill, Detail))
		{
			ShowBoundaryFeedback(NSLOCTEXT("RokCommander", "SkillDetailUnavailable",
				"此统帅的技能配置详情暂不可用。"));
			return;
		}
		SelectedCommanderSkillIndex = RequestedIndex;
		SelectedCommanderSkillDetailIndex = RequestedIndex;
		RebuildModeContent();
		UE_LOG(LogTemp, Log,
			TEXT("Rok commander skill config detail: hero=%d skill=%d local_preview=1 state_unchanged=1."),
			SelectedCommanderId, SkillId);
		return;
	}
	if (ActionId == 465 && ActiveMode == ERokUnityFeatureScreenMode::Commander)
	{
		SelectedCommanderSkillDetailIndex = INDEX_NONE;
		RebuildModeContent();
		return;
	}
	if (ActionId == 470)
	{
		bCommanderSortMenuOpen = !bCommanderSortMenuOpen;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 472 && ActionId <= 475)
	{
		SelectedCommanderSortType = ActionId - 472;
		bCommanderSortMenuOpen = false;
		SelectedCommanderSkillIndex = 0;
		SelectedCommanderSkillDetailIndex = INDEX_NONE;
		SelectedCommanderTalentIndex = INDEX_NONE;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 5000 && ActionId < 5000 + VisibleItemsStacks.Num())
	{
		const int32 RequestedIndex = ActionId - 5000;
		if (VisibleItemsStacks.IsValidIndex(RequestedIndex))
		{
			SelectedItemsIndex = RequestedIndex;
			SelectedItemsStackIndex = VisibleItemsStacks[RequestedIndex].ItemIndex;
			SelectedItemsUseCount = 1;
			RebuildModeContent();
		}
		return;
	}
	if (ActionId >= 520 && ActionId < 525)
	{
		SelectedItemsCategory = ActionId - 520;
		SelectedItemsIndex = 0;
		SelectedItemsStackIndex = 0;
		SelectedItemsUseCount = 1;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 540 && ActionId <= 542)
	{
		if (!VisibleItemsStacks.IsValidIndex(SelectedItemsIndex))
		{
			return;
		}
		const int32 MaximumUseCount = static_cast<int32>(FMath::Clamp<int64>(
			VisibleItemsStacks[SelectedItemsIndex].Overlay,
			1,
			9999));
		if (ActionId == 540)
		{
			SelectedItemsUseCount = FMath::Max(1, SelectedItemsUseCount - 1);
		}
		else if (ActionId == 541)
		{
			SelectedItemsUseCount = FMath::Min(
				MaximumUseCount,
				SelectedItemsUseCount + 1);
		}
		else
		{
			SelectedItemsUseCount = MaximumUseCount;
		}
		RebuildModeContent();
		return;
	}
	if (ActionId == 543)
	{
		if (!VisibleItemsStacks.IsValidIndex(SelectedItemsIndex))
		{
			ShowBoundaryFeedback(NSLOCTEXT(
				"RokItems",
				"NoItemSelection",
				"当前没有可使用的道具。"));
		}
		else
		{
			const FItemsInventoryStackState& Stack =
				VisibleItemsStacks[SelectedItemsIndex];
			UE_LOG(
				LogTemp,
				Log,
				TEXT("Rok items use boundary: item_index=%lld item_id=%d item_num=%d overlay=%lld state_unchanged=1 authority=server."),
				static_cast<long long>(Stack.ItemIndex),
				Stack.ItemId,
				SelectedItemsUseCount,
				static_cast<long long>(Stack.Overlay));
			ShowBoundaryFeedback(NSLOCTEXT(
				"RokItems",
				"UseBoundary",
				"道具使用请求尚未接入服务器，背包数量未改变。"));
		}
		return;
	}
	if (ActionId == 600)
	{
		AlliancePreviewState = 1;
		RebuildModeContent();
		return;
	}
	if (ActionId == 601)
	{
		ResetAllianceCreateLocalDraft();
		AlliancePreviewState = 3;
		RebuildModeContent();
		if (GetAllianceFlagEditorPreviewTab() != INDEX_NONE)
		{
			ClickAllianceFlagEditorPreviewButton(*this, TEXT("AllianceFlagEdit"));
		}
		return;
	}
	if (ActionId == 633 && ActiveMode == ERokUnityFeatureScreenMode::Alliance
		&& AlliancePreviewState == 0 && !bAllianceTechnologyReferenceOpen)
	{
		bAllianceOfficeReferenceOpen = true;
		RebuildModeContent();
		UE_LOG(LogTemp, Display,
			TEXT("Rok alliance office reference: opened from welcome; alliance_state_unchanged=1 authority=local_reference."));
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Alliance && !bAllianceTechnologyReferenceOpen
		&& !bAllianceOfficeReferenceOpen
		&& ((ActionId == 636 && AlliancePreviewState == 0)
			|| (ActionId == 655 && AlliancePreviewState == 2 && SelectedAlliancePage == 0)))
	{
		bAllianceTechnologyReferenceOpen = true;
		SelectedAllianceTechnologyCategory = 1;
		SelectedAllianceTechnologyStudyType = 0;
		SelectedAllianceTechnologyLevelKey = 1;
		RebuildModeContent();
		return;
	}
	if (ActionId == 635 && ActiveMode == ERokUnityFeatureScreenMode::Alliance
		&& AlliancePreviewState == 2 && SelectedAlliancePage == 1)
	{
		bAllianceOfficeReferenceOpen = true;
		RebuildModeContent();
		UE_LOG(LogTemp, Display,
			TEXT("Rok alliance office reference: opened from members; alliance_state_unchanged=1 authority=local_reference."));
		return;
	}
	if (ActionId == 634 && ActiveMode == ERokUnityFeatureScreenMode::Alliance
		&& bAllianceOfficeReferenceOpen)
	{
		bAllianceOfficeReferenceOpen = false;
		UE_LOG(LogTemp, Display,
			TEXT("Rok alliance office reference: closed; alliance_state_unchanged=1 authority=local_reference."));
		RebuildModeContent();
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Alliance && bAllianceTechnologyReferenceOpen)
	{
		if (ActionId == 680)
		{
			bAllianceTechnologyReferenceOpen = false;
			RebuildModeContent();
			return;
		}
		if (ActionId >= 681 && ActionId <= 683)
		{
			SelectedAllianceTechnologyCategory = ActionId - 680;
			SelectedAllianceTechnologyStudyType = 0;
			SelectedAllianceTechnologyLevelKey = 1;
			RebuildModeContent();
			return;
		}
		URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
		if (Catalog && Catalog->LoadAllianceTechnologyReadonly())
		{
			const TArray<FRokUnityAllianceTechnologyReadonlyRow>& Rows = Catalog->GetAllianceTechnologyReadonlyRows();
			if (ActionId >= 17001 && ActionId <= 17322)
			{
				for (const FRokUnityAllianceTechnologyReadonlyRow& Row : Rows)
				{
					if (Row.StudyType == ActionId - 17000 && Row.LevelKey == 1
						&& Row.CategoryType == SelectedAllianceTechnologyCategory)
					{
						SelectedAllianceTechnologyStudyType = Row.StudyType;
						SelectedAllianceTechnologyLevelKey = 1;
						RebuildModeContent();
						return;
					}
				}
				return;
			}
			if (ActionId == 684 || ActionId == 685)
			{
				const int32 NextLevel = SelectedAllianceTechnologyLevelKey + (ActionId == 684 ? -1 : 1);
				for (const FRokUnityAllianceTechnologyReadonlyRow& Row : Rows)
				{
					if (Row.StudyType == SelectedAllianceTechnologyStudyType && Row.LevelKey == NextLevel
						&& Row.CategoryType == SelectedAllianceTechnologyCategory)
					{
						SelectedAllianceTechnologyLevelKey = NextLevel;
						RebuildModeContent();
						return;
					}
				}
				return;
			}
		}
		if (ActionId >= 680 && ActionId <= 685) return;
	}
	if (ActionId >= 610 && ActionId < 615)
	{
		const int32 OriginalIndex = ActionId - 610;
		if (ActiveMode == ERokUnityFeatureScreenMode::Alliance && AlliancePreviewState == 1
			&& bAllianceTestFixtureState && VisibleAllianceFixtureIndices.Contains(OriginalIndex))
		{
			if (const UEditableTextBox* SearchInput =
				Cast<UEditableTextBox>(GetWidgetFromName(TEXT("AllianceSearchInput"))))
			{
				AllianceFixtureSearchInput = SearchInput->GetText().ToString();
			}
			SelectedAllianceListIndex = OriginalIndex;
			RebuildModeContent();
		}
		return;
	}
	if (ActionId == 620)
	{
		if (bAllianceTestFixtureState && AlliancePreviewState == 1
			&& !VisibleAllianceFixtureIndices.Contains(SelectedAllianceListIndex))
		{
			ShowBoundaryFeedback(NSLOCTEXT("RokAlliance", "FixtureSelectBeforeJoin",
				"请先选择本地参考列表中的联盟。"));
			return;
		}
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Rok alliance join boundary: fixture=%d selected_index=%d state_unchanged=1 authority=server."),
			bAllianceTestFixtureState ? 1 : 0,
			SelectedAllianceListIndex);
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokAlliance",
			"JoinBoundary",
			"加入联盟需要服务器确认，当前联盟状态未改变。"));
		return;
	}
	if (ActionId == 623)
	{
		auto ReadAllianceInput = [this](const FName WidgetName)
		{
			if (const UEditableTextBox* Input =
					Cast<UEditableTextBox>(GetWidgetFromName(WidgetName)))
			{
				return Input->GetText().ToString();
			}
			return FString();
		};
		FString Name = ReadAllianceInput(TEXT("AllianceCreateNameInput"));
		FString Tag = ReadAllianceInput(TEXT("AllianceCreateTagInput"));
		FString Notice = ReadAllianceInput(TEXT("AllianceCreateNoticeInput"));
		Name.TrimStartAndEndInline();
		Tag.TrimStartAndEndInline();
		Notice.TrimStartAndEndInline();
		FRokUnityAllianceCreateConfig AllianceConfig;
		if (URokUnityEconomyCatalogSubsystem* Catalog = GetEconomyCatalogSubsystem())
		{
			if (!Catalog->IsCatalogLoaded())
			{
				Catalog->LoadCatalog();
			}
			if (Catalog->IsCatalogLoaded())
			{
				AllianceConfig = Catalog->GetAllianceCreateConfig();
			}
		}
		if (Name.IsEmpty() || Tag.IsEmpty())
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("Rok alliance create validation: name_length=%d tag_length=%d state_unchanged=1."),
				Name.Len(),
				Tag.Len());
			ShowBoundaryFeedback(NSLOCTEXT(
				"RokAlliance",
				"CreateRequiredFields",
				"请输入联盟名称和简称。"));
			return;
		}
		if (AllianceConfig.NameMinCharacters > 0
			&& (Name.Len() < AllianceConfig.NameMinCharacters
				|| Name.Len() > AllianceConfig.NameMaxCharacters))
		{
			ShowBoundaryFeedback(FText::Format(
				NSLOCTEXT("RokAlliance", "CreateNameRange", "联盟名称需要{0}-{1}个字符。"),
				FText::AsNumber(AllianceConfig.NameMinCharacters),
				FText::AsNumber(AllianceConfig.NameMaxCharacters)));
			return;
		}
		if (AllianceConfig.TagMinCharacters > 0
			&& (Tag.Len() < AllianceConfig.TagMinCharacters
				|| Tag.Len() > AllianceConfig.TagMaxCharacters))
		{
			ShowBoundaryFeedback(FText::Format(
				NSLOCTEXT("RokAlliance", "CreateTagRange", "联盟简称需要{0}-{1}个字符。"),
				FText::AsNumber(AllianceConfig.TagMinCharacters),
				FText::AsNumber(AllianceConfig.TagMaxCharacters)));
			return;
		}
		if (AllianceConfig.NoticeMaxCharacters > 0
			&& Notice.Len() > AllianceConfig.NoticeMaxCharacters)
		{
			ShowBoundaryFeedback(FText::Format(
				NSLOCTEXT("RokAlliance", "CreateNoticeRange", "联盟公告不能超过{0}个字符。"),
				FText::AsNumber(AllianceConfig.NoticeMaxCharacters)));
			return;
		}
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Rok alliance create boundary: name_length=%d tag_length=%d notice_length=%d state_unchanged=1 authority=server."),
			Name.Len(),
			Tag.Len(),
			Notice.Len());
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokAlliance",
			"CreateBoundary",
			"创建联盟需要服务器确认，当前联盟状态未改变。"));
		return;
	}
	if (ActionId == 621 || ActionId == 622)
	{
		if (ActionId == 622) ResetAllianceCreateLocalDraft();
		AlliancePreviewState = 0;
		AllianceFixtureSearchInput.Reset();
		AllianceFixtureSearchQuery.Reset();
		VisibleAllianceFixtureIndices.Reset();
		SelectedAllianceListIndex = 0;
		RebuildModeContent();
		return;
	}
	if (ActionId == 624)
	{
		FString Query;
		if (const UEditableTextBox* SearchInput =
				Cast<UEditableTextBox>(GetWidgetFromName(TEXT("AllianceSearchInput"))))
		{
			Query = SearchInput->GetText().ToString();
		}
		if (bAllianceTestFixtureState && ActiveMode == ERokUnityFeatureScreenMode::Alliance
			&& AlliancePreviewState == 1
			&& FParse::Param(FCommandLine::Get(), TEXT("RokAlliancePreviewFixture")))
		{
			AllianceFixtureSearchInput = Query;
			AllianceFixtureSearchQuery = Query;
			AllianceFixtureSearchQuery.TrimStartAndEndInline();
			SelectedAllianceListIndex = INDEX_NONE;
			RebuildModeContent();
			return;
		}
		Query.TrimStartAndEndInline();
		int32 SearchMinCharacters = 0;
		if (URokUnityEconomyCatalogSubsystem* Catalog = GetEconomyCatalogSubsystem())
		{
			if (!Catalog->IsCatalogLoaded())
			{
				Catalog->LoadCatalog();
			}
			if (Catalog->IsCatalogLoaded())
			{
				SearchMinCharacters =
					Catalog->GetAllianceCreateConfig().SearchMinCharacters;
			}
		}
		if (!Query.IsEmpty()
			&& SearchMinCharacters > 0
			&& Query.Len() < SearchMinCharacters)
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("Rok alliance search validation: query_length=%d minimum=%d state_unchanged=1."),
				Query.Len(),
				SearchMinCharacters);
			ShowBoundaryFeedback(FText::Format(
				NSLOCTEXT("RokAlliance", "SearchMinimum", "请至少输入{0}个字符。"),
				FText::AsNumber(SearchMinCharacters)));
			return;
		}
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Rok alliance search boundary: query_length=%d state_unchanged=1 authority=server."),
			Query.Len());
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokAlliance",
			"SearchBoundary",
			"联盟搜索需要服务器数据，当前列表未改变。"));
		return;
	}
	if (ActionId == 657 && ActiveMode == ERokUnityFeatureScreenMode::Alliance
		&& AlliancePreviewState == 2 && SelectedAlliancePage == 0
		&& !bAllianceOfficeReferenceOpen && !bAllianceTechnologyReferenceOpen
		&& !bAllianceGiftReferenceOpen)
	{
		bAllianceShopReferenceOpen = true;
		SelectedAllianceShopGroup = 1;
		SelectedAllianceShopItemType = 0;
		RebuildModeContent();
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Alliance && bAllianceShopReferenceOpen)
	{
		if (ActionId == 690)
		{
			bAllianceShopReferenceOpen = false;
			RebuildModeContent();
			return;
		}
		if (ActionId >= 691 && ActionId <= 694)
		{
			SelectedAllianceShopGroup = ActionId - 690;
			SelectedAllianceShopItemType = 0;
			RebuildModeContent();
			return;
		}
		if (ActionId >= 18100 && ActionId < 18121)
		{
			URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
			if (Catalog && Catalog->LoadAllianceShopReadonly())
			{
				const TArray<FRokUnityAllianceShopReadonlyRow>& Rows = Catalog->GetAllianceShopReadonlyRows();
				const int32 Index = ActionId - 18100;
				if (Rows.IsValidIndex(Index) && Rows[Index].Group == SelectedAllianceShopGroup)
				{
					SelectedAllianceShopItemType = Rows[Index].ItemType;
					RebuildModeContent();
				}
			}
			return;
		}
	}
	if (ActionId == 656 && ActiveMode == ERokUnityFeatureScreenMode::Alliance
		&& AlliancePreviewState == 2 && SelectedAlliancePage == 0
		&& !bAllianceOfficeReferenceOpen && !bAllianceTechnologyReferenceOpen)
	{
		bAllianceGiftReferenceOpen = true;
		SelectedAllianceGiftConfiguredLevel = 1;
		bAllianceGiftSpecialDefinitions = false;
		RebuildModeContent();
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Alliance && bAllianceGiftReferenceOpen)
	{
		if (ActionId == 686)
		{
			bAllianceGiftReferenceOpen = false;
			RebuildModeContent();
			return;
		}
		if (ActionId == 687 || ActionId == 688)
		{
			SelectedAllianceGiftConfiguredLevel = FMath::Clamp(
				SelectedAllianceGiftConfiguredLevel + (ActionId == 687 ? -1 : 1), 1, 50);
			bAllianceGiftSpecialDefinitions = false;
			RebuildModeContent();
			return;
		}
		if (ActionId == 689)
		{
			bAllianceGiftSpecialDefinitions = !bAllianceGiftSpecialDefinitions;
			RebuildModeContent();
			return;
		}
	}
	if (ActionId == 625)
	{
		if (ActiveMode == ERokUnityFeatureScreenMode::Alliance && AlliancePreviewState == 3
			&& !bAllianceFlagEditorOpen)
		{
			if (!RokAllianceFlagLocalPreview::IsValidDraft(AllianceCreateFlagDraft)) return;
			CaptureAllianceCreateFormDraft();
			AllianceFlagEditDraft = AllianceCreateFlagDraft;
			SelectedAllianceFlagType = 1;
			bAllianceFlagEditorOpen = true;
			RebuildModeContent();
			const int32 PreviewType = GetAllianceFlagEditorPreviewTab();
			if (PreviewType != INDEX_NONE)
			{
				ClickAllianceFlagEditorPreviewButton(*this, *FString::Printf(TEXT("AllianceFlagTab_%d"), PreviewType));
			}
			return;
		}
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Rok alliance flag boundary: state_unchanged=1 authority=server."));
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokAlliance",
			"FlagBoundary",
			"联盟旗帜编辑暂未开放。"));
		return;
	}
	if (ActionId >= 630 && ActionId < 633)
	{
		SelectedAlliancePage = ActionId - 630;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 640 && ActionId <= 643 && ActiveMode == ERokUnityFeatureScreenMode::Alliance
		&& (bAllianceOfficeReferenceOpen
			|| (AlliancePreviewState == 2 && SelectedAlliancePage == 1)))
	{
		SelectedAllianceOfficeIndex = ActionId - 640;
		RebuildModeContent();
		UE_LOG(LogTemp, Display,
			TEXT("Rok alliance office definition selected: index=%d state_unchanged=1 authority=local_reference."),
			SelectedAllianceOfficeIndex);
		return;
	}
	if (ActionId == 675 && bAllianceTestFixtureState)
	{
		// Fixture-only navigation: Unity quit-alliance is a server call behind
		// a confirm dialog; the fixture uses it to walk the unjoined flow
		// without pretending the account actually left an alliance.
		AlliancePreviewState = 0;
		SelectedAlliancePage = 0;
		RebuildModeContent();
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok alliance fixture nav: quit_preview_to_welcome authority=local_fixture."));
		return;
	}
	if (ActionId == 651 && ActiveMode == ERokUnityFeatureScreenMode::Alliance
		&& AlliancePreviewState == 2 && SelectedAlliancePage == 0
		&& !bAllianceOfficeReferenceOpen && !bAllianceTechnologyReferenceOpen
		&& !bAllianceGiftReferenceOpen && !bAllianceShopReferenceOpen
		&& !bAllianceFlagEditorOpen)
	{
		bAllianceTerritoryReferenceOpen = true;
		ExpandedAllianceTerritoryGroup = INDEX_NONE;
		SelectedAllianceTerritoryType = 0;
		RebuildModeContent();
		UE_LOG(LogTemp, Display, TEXT("Rok alliance territory reference: static_config_only=1 owned_state=0 economy=0."));
		return;
	}
	if (ActionId == 672 && ActiveMode == ERokUnityFeatureScreenMode::Alliance
		&& AlliancePreviewState == 2 && SelectedAlliancePage == 2
		&& !bAllianceOfficeReferenceOpen && !bAllianceTechnologyReferenceOpen
		&& !bAllianceTerritoryReferenceOpen && !bAllianceGiftReferenceOpen
		&& !bAllianceShopReferenceOpen && !bAllianceFlagEditorOpen)
	{
		// 672 is this UE settings menu's local action, not a Unity/backend ID.
		bAllianceRankingReferenceOpen = true;
		SelectedAllianceRankingCategoryId = INDEX_NONE;
		RebuildModeContent();
		return;
	}
	if ((ActionId >= 650 && ActionId <= 660) || (ActionId >= 670 && ActionId < 676))
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Rok alliance feature boundary: action=%d state_unchanged=1 authority=server."),
			ActionId);
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokAlliance",
			"FeatureBoundary",
			"该联盟功能暂未开放。"));
		return;
	}
	if (ActionId == 700)
	{
		SelectedVipPreviewLevel = FMath::Max(0, SelectedVipPreviewLevel - 1);
		RebuildModeContent();
		return;
	}
	if (ActionId == 701)
	{
		SelectedVipPreviewLevel = FMath::Min(VipLevelCount - 1, SelectedVipPreviewLevel + 1);
		RebuildModeContent();
		return;
	}
	if (ActionId == 740 || ActionId == 743)
	{
		RequestVipClaim(ActionId == 743);
		return;
	}
	if (ActionId == 741)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok VIP boundary: action=add_points state_unchanged=1 authority=server."));
		ShowBoundaryFeedback(NSLOCTEXT("RokVip", "AddPointsBoundary", "VIP点数购买与道具使用需要服务器确认，当前进度未改变。"));
		return;
	}
	if (ActionId == 742)
	{
		FText HelpText;
		if (URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem())
		{
			if (Catalog->GetVipDailyPointHelpText(HelpText))
			{
				bVipDailyPointHelpVisible = !bVipDailyPointHelpVisible;
				RebuildModeContent();
				return;
			}
		}
		ShowBoundaryFeedback(FText::Format(
			NSLOCTEXT("RokVip", "HelpBoundary", "VIP {0} 权益预览来自 Unity VIP 配置；账号进度与奖励状态由服务器同步。"),
			FText::AsNumber(SelectedVipPreviewLevel)));
		return;
	}
	if (ActionId == 745)
	{
		// The source HelpTip stays open on its own body; clicking elsewhere closes it.
		return;
	}
	if (ActionId == 744)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok VIP boundary: action=special_chest_purchase state_unchanged=1 authority=server."));
		ShowBoundaryFeedback(NSLOCTEXT("RokVip", "PurchaseBoundary", "特惠宝箱购买需要支付与服务器确认，当前未创建订单。"));
		return;
	}
	if (ActionId >= 800 && ActionId < 803)
	{
		if (ChatInputTextBox)
		{
			ChatDraft = ChatInputTextBox->GetText().ToString();
		}
		SelectedChatChannel = ActionId - 800;
		ChatMessageScrollOffset = 0.0f;
		bChatEmojiPanelOpen = false;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok chat channel switch: channel=%d local_ui_state=1."),
			SelectedChatChannel);
		RebuildModeContent();
		return;
	}
	if (ActionId == 810)
	{
		if (ChatInputTextBox)
		{
			ChatDraft = ChatInputTextBox->GetText().ToString();
		}
		ChatDraft.TrimStartAndEndInline();
		if (ChatDraft.IsEmpty())
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok chat send blocked: channel=%d message_length=0 reason=empty state_unchanged=1 authority=client_validation."),
				SelectedChatChannel);
			ShowBoundaryFeedback(
				NSLOCTEXT(
					"RokChat",
					"EmptyMessage",
					"请输入聊天内容。"));
			return;
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok chat send boundary: channel=%d message_length=%d state_unchanged=1 authority=server."),
			SelectedChatChannel,
			ChatDraft.Len());
		ShowBoundaryFeedback(
			NSLOCTEXT(
				"RokChat",
				"SendBoundary",
				"消息等待聊天服务确认，当前聊天记录未改变。"));
		return;
	}
	if (ActionId == 811)
	{
		if (auto* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ChatMessageScroll"))))
			ChatMessageScrollOffset = Scroll->GetScrollOffset();
		if (ChatInputTextBox)
		{
			ChatDraft = ChatInputTextBox->GetText().ToString();
		}
		bChatEmojiPanelOpen = !bChatEmojiPanelOpen;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok chat emoji panel: open=%d local_ui_state=1."),
			bChatEmojiPanelOpen ? 1 : 0);
		RebuildModeContent();
		return;
	}
	if (ActionId == 813 && ActiveMode == ERokUnityFeatureScreenMode::Chat)
	{
		if (auto* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("ChatMessageScroll"))))
			ChatMessageScrollOffset = Scroll->GetScrollOffset();
		if (ChatInputTextBox) ChatDraft = ChatInputTextBox->GetText().ToString();
		bChatEmojiPanelOpen = false;
		RebuildModeContent();
		return;
	}
	if (ActionId == 812 && ActiveMode == ERokUnityFeatureScreenMode::Chat)
	{
		ChatContactAnimation.Toggle();
		ApplyWindowAnimation();
		return;
	}
	if (ActionId == 820 || ActionId == 821)
	{
		if (ChatInputTextBox)
		{
			ChatDraft = ChatInputTextBox->GetText().ToString();
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok chat emoji send boundary: channel=%d emoji=%d state_unchanged=1 authority=server."),
			SelectedChatChannel,
			ActionId - 820);
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokChat",
			"EmojiSendBoundary",
			"表情发送等待聊天服务确认。"));
		return;
	}
	if (ActionId >= 850 && ActionId <= 853)
	{
		FIntPoint Minimum = FIntPoint::ZeroValue;
		FIntPoint Maximum = FIntPoint::ZeroValue;
		bool bLocalFallback = false;
		if (!ResolveWorldPositionDisplayBounds(GetWorld(), Minimum, Maximum,
			bLocalFallback))
		{
			ShowBoundaryFeedback(NSLOCTEXT("RokWorldPosition", "InvalidProjection",
				"当前地图坐标范围不可用。"));
			return;
		}
		switch (ActionId)
		{
		case 850:
			SearchCoordinateX = static_cast<int32>(FMath::Max<int64>(
				Minimum.X, static_cast<int64>(SearchCoordinateX) - 1));
			break;
		case 851:
			SearchCoordinateX = static_cast<int32>(FMath::Min<int64>(
				Maximum.X, static_cast<int64>(SearchCoordinateX) + 1));
			break;
		case 852:
			SearchCoordinateY = static_cast<int32>(FMath::Max<int64>(
				Minimum.Y, static_cast<int64>(SearchCoordinateY) - 1));
			break;
		default:
			SearchCoordinateY = static_cast<int32>(FMath::Min<int64>(
				Maximum.Y, static_cast<int64>(SearchCoordinateY) + 1));
			break;
		}
		RebuildModeContent();
		return;
	}
	if (ActionId == 854)
	{
		if (!WorldPositionXInput || !WorldPositionYInput)
		{
			ShowBoundaryFeedback(NSLOCTEXT("RokWorldPosition", "MissingInputs",
				"坐标输入暂不可用。"));
			return;
		}
		const FString RawX = WorldPositionXInput->GetText().ToString();
		const FString RawY = WorldPositionYInput->GetText().ToString();
		const FString TextX = RawX.TrimStartAndEnd();
		const FString TextY = RawY.TrimStartAndEnd();
		if (TextX.IsEmpty() || TextY.IsEmpty())
		{
			// Unity Tip 100716. Whitespace is also empty in this local
			// validation; Unity's stale-value behavior is intentionally excluded.
			ShowWorldPositionMiddleTip(NSLOCTEXT("RokWorldPosition", "EmptyCoordinate",
				"未输入坐标"));
			return;
		}
		int32 ParsedX = 0;
		int32 ParsedY = 0;
		if (!ParseWorldPositionInt32Strict(TextX, ParsedX)
			|| !ParseWorldPositionInt32Strict(TextY, ParsedY))
		{
			ShowBoundaryFeedback(NSLOCTEXT("RokWorldPosition", "InvalidCoordinate",
				"请输入有效坐标。"));
			return;
		}
		FIntPoint Minimum = FIntPoint::ZeroValue;
		FIntPoint Maximum = FIntPoint::ZeroValue;
		bool bLocalFallback = false;
		if (!ResolveWorldPositionDisplayBounds(GetWorld(), Minimum, Maximum,
			bLocalFallback))
		{
			ShowBoundaryFeedback(NSLOCTEXT("RokWorldPosition", "InvalidProjection",
				"当前地图坐标范围不可用。"));
			return;
		}
		// Update the stored pair only after both current text fields are valid.
		SearchCoordinateX = FMath::Clamp(ParsedX, Minimum.X, Maximum.X);
		SearchCoordinateY = FMath::Clamp(ParsedY, Minimum.Y, Maximum.Y);
		if (APlayerController* Controller = GetOwningPlayer())
		{
			if (ARokWorldMapCameraPawn* CameraPawn =
					Cast<ARokWorldMapCameraPawn>(Controller->GetPawn()))
			{
				const FVector2D TargetCoordinate(
					SearchCoordinateX,
					SearchCoordinateY);
				CameraPawn->FocusDisplayCoordinate(TargetCoordinate);
				UE_LOG(
					LogTemp,
					Display,
					TEXT("Rok world position search focused X=%d Y=%d."),
					SearchCoordinateX,
					SearchCoordinateY);
				CloseScreen();
				return;
			}
			if (ARokStrategyCameraPawn* CameraPawn =
					Cast<ARokStrategyCameraPawn>(Controller->GetPawn()))
			{
				const FVector2D TargetCoordinate(
					SearchCoordinateX,
					SearchCoordinateY);
				if (CameraPawn->RequestReturnToWorldMapAtDisplayCoordinate(
						TargetCoordinate))
				{
					UE_LOG(
						LogTemp,
						Display,
						TEXT("Rok city position search requested world-map focus X=%d Y=%d."),
						SearchCoordinateX,
						SearchCoordinateY);
					CloseScreen();
					return;
				}
			}
		}
		ShowBoundaryFeedback(
			NSLOCTEXT(
				"RokWorldPosition",
				"WorldOnlyBoundary",
				"当前场景无法打开世界地图坐标。"));
		return;
	}
	if (ActionId >= 900 && ActionId < 905)
	{
		SelectedMapMarkerPage = ActionId - 900;
		SelectedMapMarkerIndex =
			FindFirstMarkerForPage(SelectedMapMarkerPage);
		RebuildModeContent();
		return;
	}
	if (ActionId >= 920
		&& ActionId < 920 + WorldMapPreviewMarkerCount)
	{
		const int32 MarkerIndex = ActionId - 920;
		if (DoesMarkerMatchPage(
				WorldMapPreviewMarkers[MarkerIndex],
				SelectedMapMarkerPage))
		{
			SelectedMapMarkerIndex = MarkerIndex;
			RebuildModeContent();
		}
		return;
	}
	if (ActionId == 940)
	{
		if (SelectedMapMarkerIndex < 0
			|| SelectedMapMarkerIndex >= WorldMapPreviewMarkerCount)
		{
			return;
		}
		if (APlayerController* Controller = GetOwningPlayer())
		{
			if (ARokWorldMapCameraPawn* CameraPawn =
					Cast<ARokWorldMapCameraPawn>(Controller->GetPawn()))
			{
				const FWorldMapPreviewMarker& Marker =
					WorldMapPreviewMarkers[SelectedMapMarkerIndex];
				CameraPawn->FocusDisplayCoordinate(
					Marker.DisplayCoordinate);
				UE_LOG(
					LogTemp,
					Display,
					TEXT("Rok map marker focused '%s' X=%.0f Y=%.0f."),
					Marker.Name,
					Marker.DisplayCoordinate.X,
					Marker.DisplayCoordinate.Y);
				CloseScreen();
				return;
			}
			if (ARokStrategyCameraPawn* CameraPawn =
					Cast<ARokStrategyCameraPawn>(Controller->GetPawn()))
			{
				const FWorldMapPreviewMarker& Marker =
					WorldMapPreviewMarkers[SelectedMapMarkerIndex];
				if (CameraPawn->RequestReturnToWorldMapAtDisplayCoordinate(
						Marker.DisplayCoordinate))
				{
					UE_LOG(
						LogTemp,
						Display,
						TEXT("Rok city map marker requested world-map focus '%s' X=%.0f Y=%.0f."),
						Marker.Name,
						Marker.DisplayCoordinate.X,
						Marker.DisplayCoordinate.Y);
					CloseScreen();
					return;
				}
			}
		}
		ShowBoundaryFeedback(
			NSLOCTEXT(
				"RokMapMarker",
				"WorldOnlyBoundary",
				"当前场景无法打开世界地图标记。"));
		return;
	}
	if (ActionId == 941)
	{
		ShowBoundaryFeedback(
			NSLOCTEXT(
				"RokMapMarker",
				"AddBoundary",
				"新增地图标记需要持久化和服务器同步，当前未写入伪造标记。"));
		return;
	}
	if (IsResourceSearchOpen() && ActionId >= 1000 && ActionId <= 1013)
	{
		if (ActionId == 1013) RequestCloseScreen();
		else if (bResourceSearchPending) return;
		else if (ActionId < 1005)
		{
			const int32 Type = ActionId - 1000;
			const FText Lock = GetResourceSearchLock(Type);
			if (!Lock.IsEmpty()) ShowBoundaryFeedback(Lock);
			else
			{
				bResourceSearchTaskGuide = false;
				SelectedResourceSearchType = Type;
				RebuildModeContent();
			}
		}
		else if (ActionId == 1010 || ActionId == 1011)
		{
			const int32 Level = SelectedResourceSearchType == 0 ? ResourceSearchBarbarianLevel : ResourceSearchResourceLevel;
			HandleResourceSearchSliderChanged(static_cast<float>(Level + (ActionId == 1010 ? -1 : 1)));
			if (ResourceSearchSlider)
				ResourceSearchSlider->SetValue(SelectedResourceSearchType == 0 ? ResourceSearchBarbarianLevel : ResourceSearchResourceLevel);
		}
		else if (ActionId == 1012) SubmitResourceSearch();
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Expedition && HandleExpeditionAction(ActionId)) return;
	if (ActiveMode == ERokUnityFeatureScreenMode::GatherArmy && HandleGatherArmyAction(ActionId)) return;
	if (ActionId >= 1200 && ActionId < 1204)
	{
		ShowBoundaryFeedback(
			NSLOCTEXT(
				"RokGemShop",
				"PurchaseBoundary",
				"支付、订单校验与宝石到账需要服务器确认，当前没有创建订单或修改宝石数量。"));
		return;
	}
	if (ActionId >= 1300 && ActionId < 1303)
	{
		if (SelectedBuildCityCategory == ActionId - 1300) return;
		SelectedBuildCityCategory = ActionId - 1300;
		BuildCityGuideTypeId = 0;
		// Unity calls ShowItem only during initial population, not tab refresh.
		BuildCityCardAnimationTime = 10;
		SelectedBuildCityTypeId = 0;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 1320
		&& ActionId < 1320 + VisibleBuildCityTypeIds.Num())
	{
		const int32 Index = ActionId - 1320;
		if (!VisibleBuildCityTypeIds.IsValidIndex(Index))
		{
			return;
		}
		const int32 TypeId = VisibleBuildCityTypeIds[Index];
		SelectedBuildCityTypeId = TypeId;
		FRokLocalConstructionPreview Preview;
		const URokBuildingUpgradeSubsystem* Buildings = GetWorld()
			? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
		if (!BuildableBuildCityTypeIds.Contains(TypeId)
			|| !Buildings || !Buildings->PreviewConstructionEntry(TypeId, Preview)
			|| !Preview.bSourceEligible || !OnBuildCityPlacementRequested.IsBound())
		{
			ShowBoundaryFeedback(!Preview.FailureReason.IsEmpty()
				? FText::FromString(Preview.FailureReason)
				: NSLOCTEXT("RokBuildCity", "PlacementUnavailable", "该建筑当前无法放置。"));
			return;
		}
		CloseScreen();
		OnBuildCityPlacementRequested.Broadcast(TypeId);
		return;
	}
	if (ActionId == 1399)
	{
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::WorkerQueue && (ActionId == 1400 || ActionId == 1420))
	{
		HandleWorkerQueueAction(ActionId);
		return;
	}
	if (ActionId >= 1450 && ActionId <= 1458)
	{
		if (ActiveMode != ERokUnityFeatureScreenMode::SettingsHub)
		{
			return;
		}
		switch (ActionId)
		{
		case 1450:
			OpenLanguageSettings();
			break;
		case 1453:
			OpenGeneralSettings();
			break;
		case 1454:
			OpenNotificationSettings();
			break;
		case 1457:
			OpenHelp();
			break;
		case 1456:
			OpenAccountSettings();
			break;
		default:
			UE_LOG(
				LogTemp,
				Log,
				TEXT("Rok settings hub gate: action=%d state_unchanged=1 authority=external."),
				ActionId);
			ShowBoundaryFeedback(NSLOCTEXT(
				"RokSettings",
				"ExternalGate",
				"该功能需要服务器或平台服务，当前未开放。"));
			break;
		}
		return;
	}
	if (ActionId >= 1460 && ActionId <= 1464)
	{
		if (ActiveMode != ERokUnityFeatureScreenMode::LanguageSettings) return;
		if (ActionId == 1464)
		{
			if (RokLanguageSettingsLayout::CanCloseWithoutReload(LanguageDraftId)) RequestCloseScreen();
		}
		else
		{
			LanguageDraftId = RokLanguageSettingsLayout::SourceId(ActionId-1460);
			UpdateLanguageDraftSelection(); // Source OnValueChange: draft only, no locale or preference write.
		}
		return;
	}
	if (ActionId >= 1500 && ActionId < 1503)
	{
		if (URokClientSettingsSubsystem* Settings =
				GetClientSettingsSubsystem())
		{
			Settings->SetGraphicsQualityTier(
				static_cast<ERokClientGraphicsQualityTier>(
					ActionId - 1499));
			RebuildModeContent();
		}
		else
		{
			ShowBoundaryFeedback(NSLOCTEXT(
				"RokGeneralSettings",
				"SettingsUnavailableFeedback",
				"本机设置状态尚未加载。"));
		}
		return;
	}
	if (ActionId >= 1510 && ActionId < 1513)
	{
		if (URokClientSettingsSubsystem* Settings =
				GetClientSettingsSubsystem())
		{
			Settings->SetFrameRateOption(
				static_cast<ERokClientFrameRateOption>(
					ActionId - 1509));
			RebuildModeContent();
		}
		else
		{
			ShowBoundaryFeedback(NSLOCTEXT(
				"RokGeneralSettings",
				"SettingsUnavailableFeedback",
				"本机设置状态尚未加载。"));
		}
		return;
	}
	if (ActionId == 1520 || ActionId == 1521)
	{
		if (URokClientSettingsSubsystem* Settings =
				GetClientSettingsSubsystem())
		{
			const float Delta = ActionId == 1520
				? -0.1f
				: 0.1f;
			Settings->SetMusicVolume(
				FMath::Clamp(
					Settings->GetMusicVolume() + Delta,
					0.0f,
					1.0f));
			RebuildModeContent();
		}
		return;
	}
	if (ActionId == 1530 || ActionId == 1531)
	{
		if (URokClientSettingsSubsystem* Settings =
				GetClientSettingsSubsystem())
		{
			const float Delta = ActionId == 1530
				? -0.1f
				: 0.1f;
			Settings->SetSfxVolume(
				FMath::Clamp(
					Settings->GetSfxVolume() + Delta,
					0.0f,
					1.0f));
			RebuildModeContent();
		}
		return;
	}
	if (ActionId >= 1540
		&& ActionId
			< 1540 + VisiblePersonalityToggleNames.Num())
	{
		if (URokClientSettingsSubsystem* Settings =
				GetClientSettingsSubsystem())
		{
			const FName ToggleName =
				VisiblePersonalityToggleNames[
					ActionId - 1540];
			Settings->SetPersonalityToggle(
				ToggleName,
				!Settings->GetPersonalityToggle(
					ToggleName));
			RebuildModeContent();
		}
		return;
	}
	if (ActionId >= 1600 && ActionId < 1603)
	{
		SelectedArmyStatisticsPage = ActionId - 1600;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 1700 && ActionId < 1704)
	{
		SelectedStoreCategory = ActionId - 1700;
		SelectedStorePage = 0;
		SelectedStoreItemIndex = 0;
		SelectedStoreQuantity = 1;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 1720 && ActionId < 1732)
	{
		const int32 LocalIndex = ActionId - 1720;
		if (VisibleStoreItemIds.IsValidIndex(LocalIndex))
		{
			SelectedStoreItemIndex = LocalIndex;
			SelectedStoreQuantity = 1;
			RebuildModeContent();
		}
		return;
	}
	if (ActionId == 1740 || ActionId == 1741)
	{
		SelectedStorePage = FMath::Max(
			0,
			SelectedStorePage
				+ (ActionId == 1740 ? -1 : 1));
		SelectedStoreItemIndex = 0;
		SelectedStoreQuantity = 1;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 1750 && ActionId <= 1752)
	{
		if (ActionId == 1750)
		{
			SelectedStoreQuantity = FMath::Max(
				1,
				SelectedStoreQuantity - 1);
		}
		else if (ActionId == 1751)
		{
			SelectedStoreQuantity = FMath::Min(
				9999,
				SelectedStoreQuantity + 1);
		}
		else
		{
			SelectedStoreQuantity = 9999;
		}
		RebuildModeContent();
		return;
	}
	if (ActionId == 1753)
	{
		if (!VisibleStoreItemIds.IsValidIndex(
				SelectedStoreItemIndex))
		{
			ShowBoundaryFeedback(NSLOCTEXT(
				"RokStore",
				"NoStoreSelection",
				"当前分类没有可购买商品。"));
		}
		else
		{
			ShowBoundaryFeedback(FText::Format(
				NSLOCTEXT(
					"RokStore",
					"PurchaseBoundary",
					"已选择道具 {0} × {1}；Shop_BuyShopItem、宝石扣除与回包尚未接入，未伪造购买成功。"),
				FText::AsNumber(
					VisibleStoreItemIds[
						SelectedStoreItemIndex]),
				FText::AsNumber(
					SelectedStoreQuantity)));
		}
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::SpeedUp && ServerSpeedUpQueueIndex > 0 && ActionId >= 21000 && ActionId < 21102)
	{
		if (ActionId == 21100 || ActionId == 21101)
		{
			if (!ServerSpeedUpConfirmation) return;
			const int32 Index = ServerSpeedUpConfirmIndex;
			ServerSpeedUpConfirmation->RemoveFromParent();
			ServerSpeedUpConfirmation = nullptr;
			ServerSpeedUpConfirmIndex = INDEX_NONE;
			if (ActionId == 21101) HandleServerSpeedUpItem(Index, true);
		}
		else if (!ServerSpeedUpConfirmation) HandleServerSpeedUpItem(ActionId - 21000);
		return;
	}
	if (ActionId >= 1800 && ActionId < 1840)
	{
		const int32 LocalIndex = ActionId - 1800;
		if (!VisibleSpeedUpItemIds.IsValidIndex(LocalIndex))
		{
			ShowBoundaryFeedback(NSLOCTEXT(
				"RokSpeedUp",
				"NoSpeedItemSelection",
				"当前页没有可用的加速道具。"));
			return;
		}
		SelectedSpeedUpItemIndex = LocalIndex;
		const int32 ItemId = VisibleSpeedUpItemIds[LocalIndex];
		RebuildModeContent();
		ShowBoundaryFeedback(FText::Format(
			NSLOCTEXT(
				"RokSpeedUp",
				"UseBoundary",
				"已选择 Item {0} 用于{1}；背包数量、Role_SpeedUp 扣物与目标队列回包尚未接入，未改变倒计时。"),
			FText::AsNumber(ItemId),
			ResolveSpeedUpQueueLabel(SpeedUpQueueType)));
		return;
	}
	if (ActionId == 1840 || ActionId == 1841)
	{
		SelectedSpeedUpPage = FMath::Max(
			0,
			SelectedSpeedUpPage
				+ (ActionId == 1840 ? -1 : 1));
		SelectedSpeedUpItemIndex = 0;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 1950 && ActionId <= 1972)
	{
		if (ActiveMode != ERokUnityFeatureScreenMode::TrainArmy) return;
		if (ActionId == 1950 || ActionId == 1951)
		{
			bTrainArmyDetailVisible = ActionId == 1950;
			TrainArmyDetailAnimationTime = 0.0f;
			bTrainArmyDetailFirstTick = true;
			RebuildModeContent();
			if (bTrainArmyDetailVisible && GetTrainArmyDetailPreviewTier() != INDEX_NONE)
			{
				const int32 Tip = ReadLocalUiPreviewOption(TEXT("RokTrainArmyPreviewTip="), 1, 2);
				if (Tip != INDEX_NONE)
				{
					ClickTrainArmyPreviewButton(Tip == 1 ? TEXT("TrainArmyDetailAttrButton_0") : TEXT("TrainArmyDetailSkillButton_0"));
				}
			}
		}
		else if (ActionId == 1952)
		{
			if (TrainArmyTipLayer) TrainArmyTipLayer->RemoveFromParent();
			TrainArmyTipLayer = nullptr;
		}
		else if (ActionId >= 1960 && ActionId <= 1966) ShowTrainArmyDetailTip(ActionId - 1960, false);
		else if (ActionId >= 1970 && ActionId <= 1972) ShowTrainArmyDetailTip(ActionId - 1970, true);
		return;
	}
	if (ActionId >= 1900 && ActionId < 1905)
	{
		if (ActiveMode != ERokUnityFeatureScreenMode::TrainArmy || bTrainArmyDetailVisible) return;
		const int32 RequestedTier = ActionId - 1899;
		URokUnityGameplayCatalogSubsystem* Catalog =
			GetGameplayCatalogSubsystem();
		if (Catalog && !Catalog->IsCatalogLoaded())
		{
			Catalog->LoadCatalog();
		}
		const TArray<FRokUnityCatalogTroop> Troops = Catalog
			? Catalog->GetTroopsForArmyType(SelectedTrainArmyType)
			: TArray<FRokUnityCatalogTroop>();
		const FRokUnityCatalogTroop* Troop = Troops.FindByPredicate(
			[RequestedTier](const FRokUnityCatalogTroop& Candidate)
			{
				return Candidate.Level == RequestedTier;
			});
		if (!Troop)
		{
			ShowBoundaryFeedback(FText::Format(
				NSLOCTEXT(
					"RokTrainArmy",
					"MissingTier",
					"Unity 目录中没有 {0} T{1} 标准兵配置。"),
				ResolveTroopTypeLabel(SelectedTrainArmyType),
				FText::AsNumber(RequestedTier)));
			return;
		}
		SelectedTrainTier = RequestedTier;
		// Source ClickHead resets the requested quantity to training capacity.
		// This is the existing catalog base-capacity preview, not a server buff.
		SelectedTrainCount = FMath::Max(1, TrainArmyBaseCapacity);
		RebuildModeContent();
		if (GetTrainArmyDetailPreviewTier() == SelectedTrainTier)
		{
			ClickTrainArmyPreviewButton(TEXT("TrainArmyDetailButton"));
		}
		return;
	}
	if (ActionId >= 1920 && ActionId <= 1922)
	{
		if (ActiveMode != ERokUnityFeatureScreenMode::TrainArmy || bTrainArmyDetailVisible || !bTrainArmyQuantityEditable) return;
		const int32 Count = ActionId == 1922 ? TrainArmyBaseCapacity
			: ActionId == 1920 ? SelectedTrainCount - 1
			: SelectedTrainCount < TrainArmyBaseCapacity ? SelectedTrainCount + 1 : SelectedTrainCount;
		UpdateTrainArmyQuantity(Count);
		return;
	}
	if (ActionId == 1940)
	{
		SubmitTrainArmy();
		return;
	}
	if (ActionId >= 1941 && ActionId <= 1943 && ActiveMode == ERokUnityFeatureScreenMode::TrainArmy)
	{
		if (ActionId == 1941) OpenTrainArmyInstantConfirmation();
		else CloseTrainArmyInstantConfirmation(ActionId == 1943);
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Hospital && ActionId >= 2000 && ActionId <= 2006)
	{
		if (ActionId == 2004) SetHospitalExplanationVisible(false);
		else if (HospitalConfirmation)
		{
			if (ActionId == 2005 || ActionId == 2006)
			{
				HospitalConfirmation->RemoveFromParent();
				HospitalConfirmation = nullptr;
				if (ActionId == 2006) SubmitHospitalTreatment(true);
			}
		}
		else if (!bHospitalExplanationVisible)
		{
			if (ActionId == 2000) SetHospitalExplanationVisible(true);
			else if (ActionId == 2001) SubmitHospitalTreatment(false);
			else if (ActionId == 2002) OpenHospitalInstantConfirmation();
			else if (ActionId == 2003)
			{
				for (const auto& Unit : HospitalState.Wounded)
				{
					HospitalCounts.Add(Unit.Id, Unit.Count);
					if (auto* Quantity = HospitalQuantities.FindRef(Unit.Id).Get()) Quantity->SetCount(Unit.Count);
				}
				UpdateHospitalActions();
			}
		}
		return;
	}
	if (ActionId == 2100)
	{
		SelectedCityManagerPage = 0;
		SelectedCityManagerGroupId = 0;
		RebuildModeContent();
		UE_LOG(LogTemp, Display, TEXT("Rok city manager: back to main list."));
		return;
	}
	if (ActionId == 2101)
	{
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokCityManager",
			"RelocateBoundary",
			"迁城条件、目标地点和服务器迁移请求尚未同步，未关闭页面或移动城市。"));
		return;
	}
	if (ActionId >= 2103 && ActionId <= 2114)
	{
		static const int32 GroupIds[] = {
			201, 301, 401, 402, 403, 501,
			502, 601, 602, 603, 604, 605};
		const int32 GroupIndex = ActionId - 2103;
		SelectedCityManagerGroupId = GroupIds[GroupIndex];
		SelectedCityManagerPage =
			SelectedCityManagerGroupId == 201 ? 2 : 1;
		RebuildModeContent();
		UE_LOG(
			LogTemp, Display,
			TEXT("Rok city manager: open group=%d page=%d."),
			SelectedCityManagerGroupId,
			SelectedCityManagerPage);
		return;
	}
	if (ActionId >= 2160 && ActionId < 2200)
	{
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokCityManager",
			"BuffUseBoundary",
			"背包数量、宝石余额和 Role_AddBuff 回包尚未同步；未购买、消耗或激活增益。"));
		return;
	}
	if (ActionId >= 6200 && ActionId < 6200 + 100
		&& ActiveMode == ERokUnityFeatureScreenMode::ArmyShow)
	{
		ShowBoundaryFeedback(NSLOCTEXT("RokArmyShow", "RecallBoundary",
			"召回请求尚未接入，服务器行军状态未改变。"));
		return;
	}
	if (ActionId == 2200)
	{
		ArmyShowPanelAnimationTime = 0.0f;
		bArmyShowRulesVisible = true;
		RebuildModeContent();
		UE_LOG(LogTemp, Display, TEXT("Rok ArmyShow rules opened: language_id=200039."));
		return;
	}
	if (ActionId == 2201)
	{
		ArmyShowPanelAnimationTime = 0.0f;
		bArmyShowRulesVisible = false;
		RebuildModeContent();
		UE_LOG(LogTemp, Display, TEXT("Rok ArmyShow rules closed: restored=army_list."));
		return;
	}
	if (ActionId >= 2300 && ActionId < 2307)
	{
		SelectedChargePageType = 101 + ActionId - 2300;
		RebuildModeContent();
		return;
	}
	if (ActionId >= 2320 && ActionId < 2340)
	{
		URokUnityEconomyCatalogSubsystem* Catalog =
			GetEconomyCatalogSubsystem();
		if (Catalog && !Catalog->IsCatalogLoaded())
		{
			Catalog->LoadCatalog();
		}
		const TArray<FRokUnityEconomyChargeGemPackage> Packages =
			Catalog
				? Catalog->GetChargeGemPackages()
				: TArray<FRokUnityEconomyChargeGemPackage>();
		const int32 PackageIndex = ActionId - 2320;
		if (!Packages.IsValidIndex(PackageIndex))
		{
			ShowBoundaryFeedback(NSLOCTEXT(
				"RokCharge",
				"MissingPackage",
				"RechargeGemMall 目录中没有对应礼包，未创建订单。"));
			return;
		}
		const FRokUnityEconomyChargeGemPackage& Package =
			Packages[PackageIndex];
		ShowBoundaryFeedback(FText::Format(
			NSLOCTEXT(
				"RokCharge",
				"PurchaseBoundary",
				"已选择商品配置 {0}（{1} 宝石）；支付 SDK、本地化价格、服务器验单与到账尚未接入，未创建订单或修改余额。"),
			FText::AsNumber(Package.ProductId),
			FText::AsNumber(Package.DenarNum)));
		return;
	}
	if (ActionId == 2340)
	{
		ShowBoundaryFeedback(FText::Format(
			NSLOCTEXT(
				"RokCharge",
				"PageActionBoundary",
				"{0} 的活动进度、领取资格和商品状态等待服务器同步；当前未伪造领取或购买结果。"),
			ResolveChargePageLabel(SelectedChargePageType)));
		return;
	}
}

void URokUnityFeatureScreenWidget::HandleChatInputCommitted(
	const FText& Text,
	ETextCommit::Type CommitMethod)
{
	ChatDraft = Text.ToString();
	if (CommitMethod == ETextCommit::OnEnter)
	{
		PendingActionId = 810;
	}
}

void URokUnityFeatureScreenWidget::HandleWorldPositionXCommitted(
	const FText& Text,
	ETextCommit::Type CommitMethod)
{
	(void)CommitMethod;
	const FString Trimmed = Text.ToString().TrimStartAndEnd();
	int32 ParsedValue = 0;
	FIntPoint Minimum = FIntPoint::ZeroValue;
	FIntPoint Maximum = FIntPoint::ZeroValue;
	bool bLocalFallback = false;
	if (ParseWorldPositionInt32Strict(Trimmed, ParsedValue)
		&& ResolveWorldPositionDisplayBounds(GetWorld(), Minimum, Maximum,
			bLocalFallback) && WorldPositionXInput)
	{
		WorldPositionXInput->SetText(
			FText::FromString(FString::FromInt(
				FMath::Clamp(ParsedValue, Minimum.X, Maximum.X))));
	}
}

void URokUnityFeatureScreenWidget::HandleWorldPositionYCommitted(
	const FText& Text,
	ETextCommit::Type CommitMethod)
{
	(void)CommitMethod;
	const FString Trimmed = Text.ToString().TrimStartAndEnd();
	int32 ParsedValue = 0;
	FIntPoint Minimum = FIntPoint::ZeroValue;
	FIntPoint Maximum = FIntPoint::ZeroValue;
	bool bLocalFallback = false;
	if (ParseWorldPositionInt32Strict(Trimmed, ParsedValue)
		&& ResolveWorldPositionDisplayBounds(GetWorld(), Minimum, Maximum,
			bLocalFallback) && WorldPositionYInput)
	{
		WorldPositionYInput->SetText(
			FText::FromString(FString::FromInt(
				FMath::Clamp(ParsedValue, Minimum.Y, Maximum.Y))));
	}
}

void URokUnityFeatureScreenWidget::HandleGraphicsQualitySliderChanged(float Value)
{
	URokClientSettingsSubsystem* Settings = GetClientSettingsSubsystem();
	if (!Settings)
	{
		return;
	}
	const int32 Tier = RokGraphicsSettingsLayout::Tier(Value);
	UpdateGraphicsSettingsSlider(TEXT("GeneralSettingsGraphics"), Tier);
	PlaySourceScrollFeedback();
	Settings->SetGraphicsQualityTier(
		static_cast<ERokClientGraphicsQualityTier>(Tier));
	if (GeneralSettingsGraphicsValueText)
	{
		static const FText Labels[] = {
			NSLOCTEXT("RokGeneralSettings", "AppliedGraphicsLow", "低"),
			NSLOCTEXT("RokGeneralSettings", "AppliedGraphicsMedium", "中"),
			NSLOCTEXT("RokGeneralSettings", "AppliedGraphicsHigh", "高")};
		GeneralSettingsGraphicsValueText->SetText(Labels[Tier - 1]);
	}
	UE_LOG(LogTemp, Log, TEXT("Rok general settings applied: graphics_tier=%d local=1."), Tier);
}

void URokUnityFeatureScreenWidget::HandleFrameRateSliderChanged(float Value)
{
	URokClientSettingsSubsystem* Settings = GetClientSettingsSubsystem();
	if (!Settings)
	{
		return;
	}
	const int32 Option = RokGraphicsSettingsLayout::Tier(Value);
	UpdateGraphicsSettingsSlider(TEXT("GeneralSettingsFrameRate"), Option);
	PlaySourceScrollFeedback();
	Settings->SetFrameRateOption(
		static_cast<ERokClientFrameRateOption>(Option));
	if (GeneralSettingsFrameRateValueText)
	{
		GeneralSettingsFrameRateValueText->SetText(FText::Format(
			NSLOCTEXT("RokGeneralSettings", "AppliedFrameRateValue", "{0} FPS"),
			FText::AsNumber(FMath::RoundToInt(Settings->GetCurrentFrameRateLimit()))));
	}
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Rok general settings applied: frame_rate_option=%d limit=%.0f local=1."),
		Option,
		Settings->GetCurrentFrameRateLimit());
}

void URokUnityFeatureScreenWidget::UpdateGeneralSettingsAudioFill(const FName& Prefix, float Value)
{
	const float Width = 818.55f * FMath::Clamp(Value, 0.0f, 1.0f);
	if (UWidget* Fill = GetWidgetFromName(*(Prefix.ToString() + TEXT("Fill"))))
	{
		Fill->SetVisibility(Width > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Fill->Slot))
		{
			CanvasSlot->SetPosition(FVector2D(92.895f + 818.55f - Width,
				CanvasSlot->GetPosition().Y));
			CanvasSlot->SetSize(FVector2D(Width, 19.0f));
		}
	}
	for (int32 Patch = 0; Patch < 3; ++Patch)
	{
		if (UWidget* Widget = GetWidgetFromName(*FString::Printf(TEXT("%sFillPatch_%d"),
			*Prefix.ToString(), Patch)))
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
			{
				const float X = Patch == 0 ? 0.0f : Patch == 1 ? 5.0f : FMath::Max(5.0f, Width - 3.0f);
				const float W = Patch == 1 ? FMath::Max(0.0f, Width - 8.0f) : Patch == 0 ? 5.0f : 3.0f;
				CanvasSlot->SetPosition(FVector2D(X, 0.0f));
				CanvasSlot->SetSize(FVector2D(W, 19.0f));
			}
		}
	}
}

void URokUnityFeatureScreenWidget::PlaySourceScrollFeedback()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URokAudioRouterSubsystem* Router = GameInstance->GetSubsystem<URokAudioRouterSubsystem>())
		{
			Router->PlaySourceSfx4005();
		}
	}
}

void URokUnityFeatureScreenWidget::HandleMusicVolumeSliderChanged(float Value)
{
	if (URokClientSettingsSubsystem* Settings = GetClientSettingsSubsystem())
	{
		// Unity plays Sound_Ui_CommonScroll at the old SFX volume first.
		PlaySourceScrollFeedback();
		Settings->SetMusicVolume(FMath::Clamp(Value, 0.0f, 1.0f));
		UpdateGeneralSettingsAudioFill(TEXT("GeneralSettingsMusic"), Settings->GetMusicVolume());
	}
}

void URokUnityFeatureScreenWidget::HandleSfxVolumeSliderChanged(float Value)
{
	if (URokClientSettingsSubsystem* Settings = GetClientSettingsSubsystem())
	{
		// At zero, the final old-volume feedback is still audible as in Unity.
		PlaySourceScrollFeedback();
		Settings->SetSfxVolume(FMath::Clamp(Value, 0.0f, 1.0f));
		UpdateGeneralSettingsAudioFill(TEXT("GeneralSettingsSfx"), Settings->GetSfxVolume());
	}
}

FText URokUnityFeatureScreenWidget::GetResourceSearchLock(int32 Type) const
{
	if (Type == 0) return FText::GetEmpty();
	const auto* Instance = GetGameInstance();
	auto* Config = Instance ? Instance->GetSubsystem<URokUnityConfigSubsystem>() : nullptr;
	const auto* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	int32 StudyType = 0;
	const int32 RowId = Type * 10000 + 1;
	if (!Config || !Config->GetIntFieldByKey(TEXT("ResourceGatherType"), TEXT("ID"), RowId, TEXT("scienceReq"), StudyType)
		|| StudyType <= 0) return FText::GetEmpty();

	// PlayerProxy.IsTechnologyUnlockByType considers any synced nonnegative level unlocked.
	const auto Role = Net ? Net->GetRoleInfo() : nullptr;
	if (Role)
	{
		const TSharedPtr<FJsonObject>* Map = nullptr;
		const TSharedPtr<FJsonObject>* Row = nullptr;
		int64 Level = -1;
		if (Role->TryGetObjectField(TEXT("technologies"), Map)
			&& (*Map)->TryGetObjectField(LexToString(StudyType), Row)
			&& RokSprotoJson::TryGetInt64(*Row, TEXT("level"), Level) && Level >= 0) return FText::GetEmpty();
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (Role->TryGetArrayField(TEXT("technologies"), Rows))
		{
			for (const auto& Value : *Rows)
			{
				int64 Key = 0;
				if (Value && Value->TryGetObject(Row) && RokSprotoJson::TryGetInt64(*Row, TEXT("technologyType"), Key)
					&& Key == StudyType && RokSprotoJson::TryGetInt64(*Row, TEXT("level"), Level) && Level >= 0)
					return FText::GetEmpty();
			}
		}
	}

	int32 StudyId = 0, NameId = 0;
	Config->GetIntFieldByKey(TEXT("ResourceGatherType"), TEXT("ID"), RowId, TEXT("scienceLevReq"), StudyId);
	Config->GetIntFieldByKey(TEXT("Study"), TEXT("ID"), StudyId, TEXT("l_nameID"), NameId);
	const FString Name = FRokUiTextTable::Resolve(NameId);
	return Name.IsEmpty() ? NSLOCTEXT("RokResourceSearch", "LockedResearch", "请先在研究院完成所需研究。")
		: FText::Format(NSLOCTEXT("RokResourceSearch", "LockedByStudy", "完成研究「{0}」后解锁。"), FText::FromString(Name));
}

void URokUnityFeatureScreenWidget::SubmitResourceSearch()
{
	auto* Controller = Cast<ARokWorldMapPlayerController>(GetOwningPlayer());
	auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Controller || !Net)
	{
		ShowBoundaryFeedback(NSLOCTEXT("RokResourceSearch", "WorldRequired", "请在世界地图搜索附近目标。"));
		return;
	}
	const FText Lock = GetResourceSearchLock(SelectedResourceSearchType);
	if (!Lock.IsEmpty())
	{
		ShowBoundaryFeedback(Lock);
		return;
	}

	bResourceSearchTaskGuide = false;
	if (ResourceSearchGuide) ResourceSearchGuide->SetVisibility(ESlateVisibility::Collapsed);
	bResourceSearchPending = true;
	if (ResourceSearchSlider) ResourceSearchSlider->SetLocked(true);
	const int32 Type = SelectedResourceSearchType;
	const int32 Level = Type == 0 ? ResourceSearchBarbarianLevel : ResourceSearchResourceLevel;
	ShowBoundaryFeedback(NSLOCTEXT("RokResourceSearch", "Searching", "正在搜索附近目标…"));

	const auto OnResult = [WeakThis = TWeakObjectPtr<URokUnityFeatureScreenWidget>(this),
		WeakController = TWeakObjectPtr<ARokWorldMapPlayerController>(Controller), Serial = ResourceSearchRequestSerial, Type, Level]
		(FRokWorldMapSearchHit Hit, const FString& Error)
	{
		auto* Screen = WeakThis.Get();
		if (!Screen || !Screen->IsResourceSearchOpen() || Screen->ResourceSearchRequestSerial != Serial
			|| Screen->ResourceSearchAnimation.IsClosing()) return;
		Screen->bResourceSearchPending = false;
		if (Screen->ResourceSearchSlider) Screen->ResourceSearchSlider->SetLocked(false);
		if (!Error.IsEmpty()) Screen->ShowBoundaryFeedback(FText::FromString(Error));
		else if (Hit.ObjectId <= 0)
		{
			Screen->ShowBoundaryFeedback(FText::Format(Type == 0
				? NSLOCTEXT("RokResourceSearch", "NoBarbarian", "在您的城市附近未找到等级{0}的乱军，请稍后再试。")
				: NSLOCTEXT("RokResourceSearch", "NoResources", "在您的城市附近未找到符合等级{0}的资源点。"),
				FText::AsNumber(Level)));
		}
		else if (auto* WorldController = WeakController.Get(); WorldController
			&& WorldController->FocusWorldSearchResult(Hit.ObjectId, Hit.UnityPosition))
		{
			Screen->RequestCloseScreen();
		}
		else Screen->ShowBoundaryFeedback(NSLOCTEXT("RokResourceSearch", "FocusUnavailable", "无法定位该目标，请重试。"));
	};
	FString Error;
	if (!Net->RequestWorldMapSearch(Type, Level, OnResult, Error)) OnResult({}, Error);
}

void URokUnityFeatureScreenWidget::HandleResourceSearchSliderChanged(float Value)
{
	const int32 MaxLevel = SelectedResourceSearchType == 0 ? 25 : 6;
	const int32 RoundedLevel = FMath::Clamp(FMath::RoundToInt(Value), 1, MaxLevel);
	if (SelectedResourceSearchType == 0)
	{
		ResourceSearchBarbarianLevel = RoundedLevel;
	}
	else
	{
		ResourceSearchResourceLevel = RoundedLevel;
	}
	if (ResourceSearchLevelValueText)
	{
		ResourceSearchLevelValueText->SetText(FText::Format(
			NSLOCTEXT("RokResourceSearch", "LevelFormatRuntime", "等级:{0}"),
			FText::AsNumber(RoundedLevel)));
	}
	if (ResourceSearchSliderFill)
	{
		ResourceSearchSliderFill->SetPercent(static_cast<float>(RoundedLevel - 1) / static_cast<float>(MaxLevel - 1));
	}
	if (ResourceSearchMonsterTip)
	{
		const auto* Net = GetGameInstance() ? GetGameInstance()->GetSubsystem<URokGameNetSubsystem>() : nullptr;
		int64 DefeatedLevel = -1;
		const bool bKnown = Net && RokSprotoJson::TryGetInt64(Net->GetRoleInfo(), TEXT("barbarianLevel"), DefeatedLevel);
		const int64 AttackLevel = FMath::Clamp<int64>(DefeatedLevel, 0, 25) + 1;
		ResourceSearchMonsterTip->SetText(SelectedResourceSearchType == 0 && bKnown && RoundedLevel > AttackLevel
			? FText::Format(NSLOCTEXT("RokResourceSearch", "AttackLevelLimit", "最高可攻击等级{0}的乱军。"), FText::AsNumber(AttackLevel))
			: FText::GetEmpty());
	}
}

URokClientSettingsSubsystem*
URokUnityFeatureScreenWidget::GetClientSettingsSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<
			URokClientSettingsSubsystem>();
	}
	return nullptr;
}

URokUnityEconomyCatalogSubsystem*
URokUnityFeatureScreenWidget::GetEconomyCatalogSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<
			URokUnityEconomyCatalogSubsystem>();
	}
	return nullptr;
}

URokUnityGameplayCatalogSubsystem*
URokUnityFeatureScreenWidget::GetGameplayCatalogSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<
			URokUnityGameplayCatalogSubsystem>();
	}
	return nullptr;
}

URokUnityRuntimeStateSubsystem*
URokUnityFeatureScreenWidget::GetRuntimeStateSubsystem() const
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		return GameInstance->GetSubsystem<
			URokUnityRuntimeStateSubsystem>();
	}
	return nullptr;
}

void URokUnityFeatureScreenWidget::ShowWorldPositionMiddleTip(const FText& Message)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::WorldPositionSearch
		|| !WorldPositionMiddleTipRoot || !WorldPositionMiddleTipBackground
		|| !WorldPositionMiddleTipText) return;
	if (FeedbackText) FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	WorldPositionMiddleTipText->SetText(Message);
	WorldPositionMiddleTipText->ForceLayoutPrepass();
	// TipManager uses preferredWidth+50; first-frame fallback is for this fixed five-glyph message.
	const float MeasuredWidth = WorldPositionMiddleTipText->GetDesiredSize().X;
	const float TextWidth = MeasuredWidth > 0.0f ? MeasuredWidth
		: Message.ToString().Len() * 22.0f;
	const float Width = FMath::Clamp(TextWidth + 50.0f, 50.0f, 640.0f);
	if (UCanvasPanelSlot* TipBackgroundSlot = Cast<UCanvasPanelSlot>(WorldPositionMiddleTipBackground->Slot))
	{
		TipBackgroundSlot->SetPosition(FVector2D((640.0f - Width) * 0.5f, 0.0f));
		TipBackgroundSlot->SetSize(FVector2D(Width, 69.0f));
	}
	WorldPositionMiddleTipElapsedSeconds = 0.0f;
	WorldPositionMiddleTipRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
	AdvanceWorldPositionMiddleTip(0.0f);
	UE_LOG(LogTemp, Log, TEXT("Rok world-position Middle tip: %s"), *Message.ToString());
}

void URokUnityFeatureScreenWidget::AdvanceWorldPositionMiddleTip(float DeltaSeconds)
{
	if (WorldPositionMiddleTipElapsedSeconds < 0.0f
		|| !WorldPositionMiddleTipRoot || !WorldPositionMiddleTipBackground) return;
	WorldPositionMiddleTipElapsedSeconds += FMath::Max(0.0f, DeltaSeconds);
	const float Time = WorldPositionMiddleTipElapsedSeconds;
	if (Time >= 3.0f)
	{
		WorldPositionMiddleTipRoot->SetRenderOpacity(0.0f);
		WorldPositionMiddleTipRoot->SetVisibility(ESlateVisibility::Collapsed);
		WorldPositionMiddleTipElapsedSeconds = -1.0f;
		return;
	}
	auto Sample = [](float T, float T0, float V0, float S0, float T1, float V1, float S1)
	{
		const float Duration = T1 - T0;
		return FMath::CubicInterp(V0, S0 * Duration, V1, S1 * Duration,
			FMath::Clamp((T - T0) / Duration, 0.0f, 1.0f));
	};
	// Source lbl_message is a child of animated img_bg. This centered group applies
	// the same scale and translation to both image and text, then the source root alpha.
	const float Scale = Time < 0.13333334f
		? Sample(Time, 0.0f, 0.0f, 0.0f, 0.13333334f, 1.0f, 0.0f) : 1.0f;
	const float Alpha = Time < 0.16666667f
		? Sample(Time, 0.0f, 0.5f, 0.0f, 0.16666667f, 1.0f, 0.0f)
		: Time <= 2.25f ? 1.0f
		: Sample(Time, 2.25f, 1.0f, 0.0f, 3.0f, 0.0f, 0.0f);
	const float SourceY = Time < 1.5f
		? Sample(Time, 0.0f, -20.0f, 0.0f, 1.5f, 5.0f, 10.0f)
		: Time < 2.0f
			? Sample(Time, 1.5f, 5.0f, 10.0f, 2.0f, 10.0f, 0.0f)
			: 10.0f;
	WorldPositionMiddleTipRoot->SetRenderScale(FVector2D(Scale, Scale));
	// Unity positive anchored Y points upward; UMG screen Y points down.
	WorldPositionMiddleTipRoot->SetRenderTranslation(FVector2D(0.0f, -SourceY));
	WorldPositionMiddleTipRoot->SetRenderOpacity(Alpha);
}

void URokUnityFeatureScreenWidget::ShowBoundaryFeedback(const FText& Message)
{
	if (ActiveMode == ERokUnityFeatureScreenMode::WorldPositionSearch
		&& WorldPositionMiddleTipRoot)
	{
		WorldPositionMiddleTipRoot->SetVisibility(ESlateVisibility::Collapsed);
		WorldPositionMiddleTipElapsedSeconds = -1.0f;
	}
	if (FeedbackText)
	{
		FeedbackText->SetText(Message);
		FeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	UE_LOG(LogTemp, Log, TEXT("Rok feature screen boundary: %s"), *Message.ToString());
}

URokUnityIndexedButton* URokUnityFeatureScreenWidget::MakeActionButton(
	const FName& Name,
	int32 ActionId,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize,
	const FText& Label,
	int32 FontSize,
	const FLinearColor& TextColor,
	ESlateBrushDrawType::Type DrawAs,
	const FMargin& BrushMargin)
{
	URokUnityIndexedButton* Button = WidgetTree->ConstructWidget<URokUnityIndexedButton>(URokUnityIndexedButton::StaticClass(), Name);
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
	Button->OnIndexedClicked.AddUObject(this, &URokUnityFeatureScreenWidget::HandleIndexedAction);
	UCanvasPanel* Content = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("%sContent"), *Name.ToString()));
	Button->SetContent(Content);
	if (TexturePath && UnityRect.Width() > 0 && UnityRect.Height() > 0)
	{
		if (DrawAs == ESlateBrushDrawType::Box)
		{
			PlaceOnCanvas(Content, MakeAtlasBorder(*FString::Printf(TEXT("%sBackground"), *Name.ToString()), TexturePath, UnityRect, AtlasSize, DesiredSize, BrushMargin), FVector2D::ZeroVector, DesiredSize);
		}
		else
		{
			PlaceOnCanvas(Content, MakeAtlasImage(*FString::Printf(TEXT("%sBackground"), *Name.ToString()), TexturePath, UnityRect, AtlasSize, DesiredSize), FVector2D::ZeroVector, DesiredSize);
		}
	}
	if (!Label.IsEmpty())
	{
		UTextBlock* LabelText = MakeScreenText(*FString::Printf(TEXT("%sLabel"), *Name.ToString()), FontSize, TextColor, true, ETextJustify::Center);
		LabelText->SetText(Label);
		PlaceOnCanvas(Content, LabelText, FVector2D(0.0f, FMath::Max(1.0f, (DesiredSize.Y - static_cast<float>(FontSize) * 1.6f) * 0.5f)), FVector2D(DesiredSize.X, DesiredSize.Y - 4.0f), 2);
	}
	return Button;
}

UImage* URokUnityFeatureScreenWidget::MakeAtlasImage(
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

UCanvasPanel* URokUnityFeatureScreenWidget::MakeAtlasBorder(
	const FName& Name,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize,
	const FMargin& BrushMargin)
{
	UCanvasPanel* Panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), Name);
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!TexturePath || UnityRect.Width() <= 0 || UnityRect.Height() <= 0)
	{
		return Panel;
	}

	const int32 SourceWidth = UnityRect.Width();
	const int32 SourceHeight = UnityRect.Height();
	int32 SourceLeft = FMath::Clamp(FMath::RoundToInt(BrushMargin.Left * SourceWidth), 0, SourceWidth);
	int32 SourceRight = FMath::Clamp(FMath::RoundToInt(BrushMargin.Right * SourceWidth), 0, SourceWidth - SourceLeft);
	int32 SourceTop = FMath::Clamp(FMath::RoundToInt(BrushMargin.Top * SourceHeight), 0, SourceHeight);
	int32 SourceBottom = FMath::Clamp(FMath::RoundToInt(BrushMargin.Bottom * SourceHeight), 0, SourceHeight - SourceTop);

	float DestLeft = static_cast<float>(SourceLeft);
	float DestRight = static_cast<float>(SourceRight);
	float DestTop = static_cast<float>(SourceTop);
	float DestBottom = static_cast<float>(SourceBottom);
	if (DestLeft + DestRight > DesiredSize.X && DestLeft + DestRight > 0.0f)
	{
		const float Scale = DesiredSize.X / (DestLeft + DestRight);
		DestLeft *= Scale;
		DestRight *= Scale;
	}
	if (DestTop + DestBottom > DesiredSize.Y && DestTop + DestBottom > 0.0f)
	{
		const float Scale = DesiredSize.Y / (DestTop + DestBottom);
		DestTop *= Scale;
		DestBottom *= Scale;
	}

	const int32 SourceCenterWidth = SourceWidth - SourceLeft - SourceRight;
	const int32 SourceCenterHeight = SourceHeight - SourceTop - SourceBottom;
	const float DestCenterWidth = FMath::Max(0.0f, DesiredSize.X - DestLeft - DestRight);
	const float DestCenterHeight = FMath::Max(0.0f, DesiredSize.Y - DestTop - DestBottom);

	auto AddPatch = [&](const TCHAR* Suffix, int32 SourceX, int32 SourceY, int32 Width, int32 Height, float DestX, float DestY, float DestWidth, float DestHeight)
	{
		if (Width <= 0 || Height <= 0 || DestWidth <= 0.0f || DestHeight <= 0.0f)
		{
			return;
		}
		const FName PatchName(*FString::Printf(TEXT("%s_%s"), *Name.ToString(), Suffix));
		PlaceOnCanvas(
			Panel,
			MakeAtlasImage(PatchName, TexturePath, SpriteRect(SourceX, SourceY, Width, Height), AtlasSize, FVector2D(DestWidth, DestHeight)),
			FVector2D(DestX, DestY),
			FVector2D(DestWidth, DestHeight));
	};

	const int32 X0 = UnityRect.Min.X;
	const int32 X1 = X0 + SourceLeft;
	const int32 X2 = UnityRect.Max.X - SourceRight;
	const int32 Y0 = UnityRect.Min.Y;
	const int32 Y1 = Y0 + SourceBottom;
	const int32 Y2 = UnityRect.Max.Y - SourceTop;
	const float DX0 = 0.0f;
	const float DX1 = DestLeft;
	const float DX2 = DesiredSize.X - DestRight;
	const float DY0 = 0.0f;
	const float DY1 = DestTop;
	const float DY2 = DesiredSize.Y - DestBottom;

	AddPatch(TEXT("TopLeft"), X0, Y2, SourceLeft, SourceTop, DX0, DY0, DestLeft, DestTop);
	AddPatch(TEXT("Top"), X1, Y2, SourceCenterWidth, SourceTop, DX1, DY0, DestCenterWidth, DestTop);
	AddPatch(TEXT("TopRight"), X2, Y2, SourceRight, SourceTop, DX2, DY0, DestRight, DestTop);
	AddPatch(TEXT("Left"), X0, Y1, SourceLeft, SourceCenterHeight, DX0, DY1, DestLeft, DestCenterHeight);
	AddPatch(TEXT("Center"), X1, Y1, SourceCenterWidth, SourceCenterHeight, DX1, DY1, DestCenterWidth, DestCenterHeight);
	AddPatch(TEXT("Right"), X2, Y1, SourceRight, SourceCenterHeight, DX2, DY1, DestRight, DestCenterHeight);
	AddPatch(TEXT("BottomLeft"), X0, Y0, SourceLeft, SourceBottom, DX0, DY2, DestLeft, DestBottom);
	AddPatch(TEXT("Bottom"), X1, Y0, SourceCenterWidth, SourceBottom, DX1, DY2, DestCenterWidth, DestBottom);
	AddPatch(TEXT("BottomRight"), X2, Y0, SourceRight, SourceBottom, DX2, DY2, DestRight, DestBottom);
	return Panel;
}

UBorder* URokUnityFeatureScreenWidget::MakeSolidBorder(const FName& Name, const FLinearColor& Color)
{
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
	Border->SetPadding(FMargin(0.0f));
	Border->SetBrushColor(Color);
	Border->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Border;
}

UTextBlock* URokUnityFeatureScreenWidget::MakeScreenText(
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
	Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Text;
}

USizeBox* URokUnityFeatureScreenWidget::WrapFixedSize(UWidget* Content, const FName& Name, const FVector2D& Size)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), Name);
	Box->SetWidthOverride(Size.X);
	Box->SetHeightOverride(Size.Y);
	Box->SetContent(Content);
	return Box;
}

void URokUnityFeatureScreenWidget::PlaceOnCanvas(
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

UTexture2D* URokUnityFeatureScreenWidget::LoadUiTexture(const TCHAR* TexturePath) const
{
	if (!TexturePath)
	{
		return nullptr;
	}
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath);
	if (!Texture)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok feature screen: missing Unity texture %s"), TexturePath);
	}
	return Texture;
}

FSlateBrush URokUnityFeatureScreenWidget::MakeAtlasBrush(
	UTexture2D* Texture,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize,
	ESlateBrushDrawType::Type DrawAs,
	const FMargin& BrushMargin) const
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	// Atlas subregions that need nine-slicing are decomposed into nine images
	// by MakeAtlasBorder. A Slate Box brush cannot combine UVRegion slicing with
	// borders measured against the subregion rather than the full atlas.
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
