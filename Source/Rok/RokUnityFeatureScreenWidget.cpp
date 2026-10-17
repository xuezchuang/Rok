#include "RokUnityFeatureScreenWidget.h"
#include "RokAllianceTerritoryReadonlyData.h"
#include "RokAllianceRankingReadonlyCatalog.h"
#include "RokHospitalExplanationWidget.h"
#include "RokResearchScreenWidget.h"
#include "RokResearchCatalog.h"
#include "RokHelpWidget.h"
#include "RokMailAttachmentMeshWidget.h"
#include "RokCommanderRosterAnimation.h"
#include "RokCommanderDataAnimation.h"
#include "RokCommanderSkillReadonlyDetail.h"
#include "RokArmyShowPanelAnimation.h"
#include "RokTrainArmyDetail.h"
#include "RokTrainArmyMeshWidget.h"
#include "RokTrainArmyQuantity.h"
#include "RokTrainResourceCost.h"
#include "RokTrainParameterLayout.h"
#include "RokGraphicsSettingsLayout.h"
#include "RokLanguageSettingsLayout.h"
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
#include "Components/SizeBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
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
#include "RokClientSettingsSubsystem.h"
#include "RokAudioRouterSubsystem.h"
#include "RokCommanderMeshWidget.h"
#include "RokStrategyCameraPawn.h"
#include "RokUnityEconomyCatalogSubsystem.h"
#include "RokUnityConfigSubsystem.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapConfigActor.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"

namespace
{
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
// ExpeditionVisualAssets/v1/atlas_manifest.json: original ui_battle and task PNGs.
const TCHAR* ExpeditionBattleAtlasPath = TEXT("/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_battle_atlas.rok_expedition_battle_atlas");
const TCHAR* ExpeditionTaskPanelPath = TEXT("/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_task_panel.rok_expedition_task_panel");
const TCHAR* ExpeditionResultAtlasPath = TEXT("/Game/RokPrototype/UI/ExpeditionSource/rok_expedition_result_atlas.rok_expedition_result_atlas");
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
const TCHAR* CommanderPortraitPath = TEXT("/Game/RokPrototype/UI/CommanderAtlases/rok_commander_hero_2007_idle.rok_commander_hero_2007_idle");
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
const FIntPoint CommanderPortraitSize(900, 1024);
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
const FIntRect CommanderPortraitRect = SpriteRect(0, 0, 900, 1024);
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
	10.0f / 20.0f,
	6.0f / 23.0f,
	10.0f / 20.0f,
	7.0f / 23.0f);

constexpr float UnityReferenceWidth = 1440.0f;
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
	return Page == FRokTaskRowState::EPage::MainTask ? TEXT("main") : TEXT("side");
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

FString ResolveBuildCityPreviewPath(int32 TypeId)
{
	return FString::Printf(
		TEXT("/Game/RokPrototype/UI/BuildCityPreviews/rok_buildcity_preview_t%d_c101_a1.rok_buildcity_preview_t%d_c101_a1"),
		TypeId,
		TypeId);
}

FIntPoint ResolveBuildCityPreviewSize(int32 TypeId)
{
	switch (TypeId)
	{
	case 3: return FIntPoint(512, 459);
	case 4: return FIntPoint(512, 434);
	case 5: return FIntPoint(512, 451);
	case 7: return FIntPoint(512, 442);
	case 9: return FIntPoint(512, 490);
	case 10: return FIntPoint(512, 456);
	case 11: return FIntPoint(512, 466);
	case 12: return FIntPoint(512, 435);
	case 13: return FIntPoint(512, 426);
	case 14: return FIntPoint(512, 390);
	case 15: return FIntPoint(512, 429);
	case 16: return FIntPoint(512, 468);
	case 17: return FIntPoint(512, 418);
	case 18: return FIntPoint(512, 432);
	case 19: return FIntPoint(512, 405);
	case 20: return FIntPoint(512, 454);
	case 21: return FIntPoint(512, 470);
	case 28: return FIntPoint(512, 401);
	case 29: return FIntPoint(400, 372);
	case 30: return FIntPoint(512, 600);
	case 31: return FIntPoint(512, 463);
	case 51: return FIntPoint(333, 256);
	case 52: return FIntPoint(304, 209);
	case 53: return FIntPoint(398, 281);
	default: return FIntPoint(512, 512);
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
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenMail(bool bUseReferenceCaptureState)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Mail;
	SelectedMailTab = 0;
	SelectedMailIndex = 0;
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

void URokUnityFeatureScreenWidget::OpenTasks(bool bUseTestFixtureState)
{
	ResetTaskCompletedRowAnimation();
	TaskCompletedAnimationFixtureCallbacks = 0;
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Tasks;
	SelectedTaskPage = 1;
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
	bTaskMainLocalProjectionRendered = false;
	bTaskMainLocalConditionMet = false;
	TaskMainDisplayedCurrent = 0;
	VisibleTaskStates.Reset();
	bTaskTestFixtureState = bUseTestFixtureState;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenCommander()
{
	CommanderRosterAnimationTime = 0.0f;
	bCommanderRosterFirstTick = true;
	CommanderAnimatedPortrait = nullptr;
	CommanderAnimatedPortraitHeroId = INDEX_NONE;
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Commander;
	SelectedCommanderId = 2007;
	FParse::Value(
		FCommandLine::Get(),
		TEXT("RokCommanderHero="),
		SelectedCommanderId);
	SelectedCommanderSkillIndex = 0;
	SelectedCommanderSkillDetailIndex = INDEX_NONE;
	SelectedCommanderTalentIndex = INDEX_NONE;
	bCommanderSortMenuOpen = false;
	VisibleCommanderIds.Reset();
	VisibleCommanderTalentIds.Reset();
	VisibleCommanderSkillIds.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenItems(bool bUseTestFixtureState)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Items;
	SelectedItemsCategory = 0;
	SelectedItemsIndex = 0;
	SelectedItemsUseCount = 1;
	bItemsTestFixtureState = bUseTestFixtureState
		|| FParse::Param(FCommandLine::Get(), TEXT("RokItemsPreviewFixture"));
	VisibleItemsStacks.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
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
	if (!bVipTestFixtureState)
	{
		if (const URokUnityRuntimeStateSubsystem* RuntimeState =
			GetRuntimeStateSubsystem())
		{
			const FRokUnityRuntimePlayerProfileState ProfileState =
				RuntimeState->GetPlayerProfileState();
			if (ProfileState.bHasVipLevel)
			{
				SelectedVipPreviewLevel = ProfileState.VipLevel;
			}
		}
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

void URokUnityFeatureScreenWidget::OpenResourceSearch()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::ResourceSearch;
	SelectedResourceSearchType = 0;
	ResourceSearchBarbarianLevel = FMath::Clamp(ResourceSearchBarbarianLevel, 1, 25);
	ResourceSearchResourceLevel = FMath::Clamp(ResourceSearchResourceLevel, 1, 6);
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenExpedition()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Expedition;
	ExpeditionRows.Reset();
	ExpeditionLoadError.Reset();
	RokExpeditionStagePreviewData::LoadAll(ExpeditionRows, ExpeditionLoadError);
	SelectedExpeditionStageId = ExpeditionRows.IsEmpty() ? 0 : ExpeditionRows[0].Id;
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

void URokUnityFeatureScreenWidget::OpenBuildCity()
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::BuildCity;
	SelectedBuildCityCategory = 0;
	SelectedBuildCityTypeId = 0;
	VisibleBuildCityTypeIds.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenWorkerQueue(AActor* BuildingActor)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::WorkerQueue;
	FeatureContextActor = BuildingActor;
	WorkerQueueRefreshAccumulator = 0.0f;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
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
	HelpWidget = WidgetTree->ConstructWidget<URokHelpWidget>(URokHelpWidget::StaticClass(), TEXT("SettingsHelpWidget"));
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
				if (UnityTypeId >= 9 && UnityTypeId <= 12)
				{
					SelectedTrainArmyType = UnityTypeId - 8;
				}
			}
		}
	}
	SelectedTrainTier = 1;
	SelectedTrainCount = 1;
	bTrainArmyDetailVisible = false;
	bTrainArmyDetailFirstTick = false;
	TrainArmyDetailAnimationTime = 0.25f;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityFeatureScreenWidget::OpenSpeedUp(
	AActor* QueueActor,
	int32 QueueType)
{
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::SpeedUp;
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
}

void URokUnityFeatureScreenWidget::OpenHospital(AActor* BuildingActor)
{
	bHospitalExplanationVisible = false; HospitalExplanationWidget = nullptr; HospitalHiddenControls.Reset();
	PendingActionId = INDEX_NONE;
	ActiveMode = ERokUnityFeatureScreenMode::Hospital;
	FeatureContextActor = BuildingActor;
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
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
	ResetAllianceCreateLocalDraft();
	bAllianceRankingReferenceOpen = false;
	SelectedAllianceRankingCategoryId = INDEX_NONE;
	ResetTaskCompletedRowAnimation();
	if (ActiveMode == ERokUnityFeatureScreenMode::Hospital) { bHospitalExplanationVisible = false; HospitalExplanationWidget = nullptr; HospitalHiddenControls.Reset(); }
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
	bBuildCityInitialScrollPending = false;
#if !UE_BUILD_SHIPPING
	bFreezeBuildCityAnimationForCapture = false;
#endif
	BuildCityAnimatedRoot = nullptr;
	BuildCityAnimatedCards.Reset();
	BuildCityCardSourceIndices.Reset();
	ResourceSearchAnimation.Reset();
	ResourceSearchDockRoot = nullptr;
	ResourceSearchPopupRoot = nullptr;
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
	SpeedUpQueueDescriptionText = nullptr;
	SpeedUpQueueProgress = nullptr;
	SpeedUpQueueProgressText = nullptr;
	WorkerQueuePrimaryTitleText = nullptr;
	WorkerQueueRuntimeStateText = nullptr;
	WorkerQueuePrimaryProgress = nullptr;
	WorkerQueuePrimaryProgressText = nullptr;
	ChatInputTextBox = nullptr;
	WorldPositionXInput = nullptr;
	WorldPositionYInput = nullptr;
	ResourceSearchSlider = nullptr;
	ResourceSearchSliderFill = nullptr;
	ResourceSearchLevelValueText = nullptr;
	FeatureContextActor.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
}

void URokUnityFeatureScreenWidget::RequestCloseScreen()
{
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
	AdvanceTaskCompletedRowAnimation();
	if (ActiveMode == ERokUnityFeatureScreenMode::Expedition)
	{
		AdvanceExpeditionAnimation(InDeltaTime);
	}
	MailAttachmentPulse.Advance(InDeltaTime);
	UpdateExpandedDesignResolution(MyGeometry.GetLocalSize());
	if (ActiveMode == ERokUnityFeatureScreenMode::BuildCity && bBuildCityInitialScrollPending)
	{
		if (auto* Scroll = Cast<UScrollBox>(GetWidgetFromName(TEXT("BuildCityBuildingScroll"))))
		{
			// UScrollBox::ScrollToEnd drops requests before its Slate widget exists.
			// Unity's RTL list starts at logical item zero, at our scroll's right end.
			if (Scroll->GetCachedGeometry().GetLocalSize().X > 0)
			{
				Scroll->ScrollToEnd();
				bBuildCityInitialScrollPending = false;
			}
		}
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
	else if (ActiveMode == ERokUnityFeatureScreenMode::Tasks && SelectedTaskPage == 1)
	{
		TaskProjectionRefreshAccumulator += InDeltaTime;
		if (TaskProjectionRefreshAccumulator >= 0.25f)
		{
			TaskProjectionRefreshAccumulator = 0.0f;
			RefreshTaskMainProjection();
		}
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

	UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("FeatureScreenDimmer"));
	Dimmer->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.74f));
	Dimmer->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* DimmerSlot = RootCanvas->AddChildToCanvas(Dimmer);
	DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	DimmerSlot->SetOffsets(FMargin(0.0f));
	DimmerSlot->SetZOrder(0);

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
	ModeCanvas->ClearChildren();
	CommanderRosterAnimationWidgets.Reset();
	CommanderDataAnimationWidgets.Reset();
	ArmyShowPanelAnimationWidgets.Reset();
	if (ActiveMode != ERokUnityFeatureScreenMode::Commander)
	{
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
	SpeedUpQueueDescriptionText = nullptr;
	SpeedUpQueueProgress = nullptr;
	SpeedUpQueueProgressText = nullptr;
	WorkerQueuePrimaryTitleText = nullptr;
	WorkerQueueRuntimeStateText = nullptr;
	WorkerQueuePrimaryProgress = nullptr;
	WorkerQueuePrimaryProgressText = nullptr;
	ChatInputTextBox = nullptr;
	WorldPositionXInput = nullptr;
	WorldPositionYInput = nullptr;
	ResourceSearchSlider = nullptr;
	ResourceSearchSliderFill = nullptr;
	ResourceSearchLevelValueText = nullptr;
	if (UBorder* Dimmer = Cast<UBorder>(WidgetTree->FindWidget(TEXT("FeatureScreenDimmer"))))
	{
		Dimmer->SetBrushColor(FLinearColor(0, 0, 0,
			ActiveMode == ERokUnityFeatureScreenMode::Research ? 0.8f : 0.74f));
		Dimmer->SetVisibility(
			(ActiveMode == ERokUnityFeatureScreenMode::ResourceSearch || ActiveMode == ERokUnityFeatureScreenMode::Chat)
				? ESlateVisibility::Collapsed
				: ESlateVisibility::HitTestInvisible);
	}
	if (UWidget* InputBlocker = WidgetTree->FindWidget(TEXT("FeatureScreenInputBlocker")))
	{
		// Unity UI.s_chat uses EnumMaskStatus.kNone: uncovered HUD remains
		// hit-testable. Chat has only bounds-matched panel shields below.
		InputBlocker->SetVisibility(ActiveMode == ERokUnityFeatureScreenMode::Chat
			? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (UCanvasPanelSlot* ModeSlot = Cast<UCanvasPanelSlot>(ModeCanvas->Slot))
	{
		const float AnchorX = ActiveMode == ERokUnityFeatureScreenMode::Chat ? 0.0f : 0.5f;
		ModeSlot->SetAnchors(FAnchors(AnchorX, 0.5f));
		ModeSlot->SetAlignment(FVector2D(AnchorX, 0.5f));
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
	BuildFeedback();
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
			if (UWidget* Shield = WidgetTree->FindWidget(Name))
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

void URokUnityFeatureScreenWidget::BuildChat()
{
	// UI_Win_ChatMediator.OnPopEmojiMenu/OnHideEmojiMenu resize the
	// message region and lift pl_down by pl_emoji.rect.height immediately.
	// UI_Win_Chat.prefab pl_emoji sizeDelta.y = 289.8 (no tween).
	const float EmojiHeight = 289.8f;
	const float EmojiLift = bChatEmojiPanelOpen ? EmojiHeight : 0.0f;
	ChatContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ChatContentCanvas"));
	PlaceOnCanvas(ModeCanvas, ChatContentCanvas, FVector2D::ZeroVector,
		FVector2D(UnityReferenceWidth, UnityReferenceHeight), 0);
	// The panel blocks its own footprint, including while its controls are
	// disabled by entry/exit. Uncovered HUD is never covered by a global mask.
	const auto MakeShield = [this](const TCHAR* Name, UCanvasPanel* Parent,
		const FVector2D& Position, const FVector2D& Size, int32 ZOrder)
	{
		UButton* Shield = WidgetTree->ConstructWidget<URokChatInputShield>(URokChatInputShield::StaticClass(), Name);
		FSlateBrush NoBrush;
		NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		FButtonStyle Style;
		Style.SetNormal(NoBrush).SetHovered(NoBrush).SetPressed(NoBrush).SetDisabled(NoBrush);
		Shield->SetStyle(Style);
		PlaceOnCanvas(Parent, Shield, Position, Size, ZOrder);
	};
	MakeShield(TEXT("ChatPanelInputShield"), ChatContentCanvas, FVector2D::ZeroVector, FVector2D(1024.0f, 810.0f), -1);
	MakeShield(TEXT("ChatAnimationInputShield"), ModeCanvas, FVector2D::ZeroVector, FVector2D(1024.0f, 810.0f), 100);
	MakeShield(TEXT("ChatCloseInputShield"), ModeCanvas, FVector2D(1019.0f, 343.0f), FVector2D(48.0f, 124.0f), 100);
	// Unity UI_Win_Chat is a 1024 px drawer: 330 px contacts on the left and
	// a 694.7 px conversation region on the right. The remaining viewport is
	// deliberately left visible and hit-testable (UI.s_chat: kNone mask).
	const FVector2D FrameSize(1024.0f, 810.0f);
	const FVector2D HalfFrameSize(FrameSize.X * 0.5f, FrameSize.Y - 60.0f);
	PlaceOnCanvas(
		ChatContentCanvas,
		MakeSolidBorder(
			TEXT("ChatMainBackground"),
			FLinearColor(0.018f, 0.026f, 0.03f, 0.99f)),
		FVector2D::ZeroVector,
		FrameSize,
		0);
	UCanvasPanel* BodyLeft = MakeAtlasBorder(
		TEXT("ChatFrameBodyLeft"),
		FrameAtlasPath,
		WindowBodyRect,
		FrameAtlasSize,
		HalfFrameSize,
		WindowBodyMargin);
	PlaceOnCanvas(
		ChatContentCanvas,
		BodyLeft,
		FVector2D(0.0f, 60.0f),
		HalfFrameSize,
		1);
	UCanvasPanel* BodyRight = MakeAtlasBorder(
		TEXT("ChatFrameBodyRight"),
		FrameAtlasPath,
		WindowBodyRect,
		FrameAtlasSize,
		HalfFrameSize,
		WindowBodyMargin);
	BodyRight->SetRenderTransformPivot(FVector2D(0.5f));
	BodyRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(
		ChatContentCanvas,
		BodyRight,
		FVector2D(512.0f, 60.0f),
		HalfFrameSize,
		1);
	UCanvasPanel* HeaderLeft = MakeAtlasBorder(
		TEXT("ChatFrameHeaderLeft"),
		FrameAtlasPath,
		WindowHeaderRect,
		FrameAtlasSize,
		FVector2D(512.0f, 60.0f),
		WindowHeaderMargin);
	PlaceOnCanvas(
		ChatContentCanvas,
		HeaderLeft,
		FVector2D::ZeroVector,
		FVector2D(512.0f, 60.0f),
		5);
	UCanvasPanel* HeaderRight = MakeAtlasBorder(
		TEXT("ChatFrameHeaderRight"),
		FrameAtlasPath,
		WindowHeaderRect,
		FrameAtlasSize,
		FVector2D(512.0f, 60.0f),
		WindowHeaderMargin);
	HeaderRight->SetRenderTransformPivot(FVector2D(0.5f));
	HeaderRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(
		ChatContentCanvas,
		HeaderRight,
		FVector2D(512.0f, 0.0f),
		FVector2D(512.0f, 60.0f),
		5);
	UTextBlock* WindowTitle = MakeScreenText(
		TEXT("ChatWindowTitle"),
		30,
		FLinearColor(0.96f, 0.94f, 0.88f, 1.0f),
		true,
		ETextJustify::Center);
	WindowTitle->SetText(NSLOCTEXT("RokChat", "ChatTitle", "聊天"));
	PlaceOnCanvas(
		ChatContentCanvas,
		WindowTitle,
		FVector2D(0.0f, 8.0f),
		FVector2D(1024.0f, 42.0f),
		7);
	if (bChatTestFixtureState)
	{
		// Chat history and contacts are server data; the fixture preview must
		// stay explicitly labeled instead of reading as account state.
		UTextBlock* ChatFixtureTag = MakeScreenText(
			TEXT("ChatFixtureTag"),
			14,
			FLinearColor(0.62f, 0.62f, 0.58f, 1.0f),
			false,
			ETextJustify::Right);
		ChatFixtureTag->SetText(NSLOCTEXT(
			"RokChat",
			"FixtureTag",
			"聊天 · 本地参考态（非账号状态）"));
		PlaceOnCanvas(
			ChatContentCanvas,
			ChatFixtureTag,
			FVector2D(704.0f, 14.0f),
			FVector2D(312.0f, 24.0f),
			7);
	}
	PlaceOnCanvas(
		ChatContentCanvas,
		MakeActionButton(
			TEXT("FeatureFrameClose"),
			1,
			ChatAtlasPath,
			ChatCloseRect,
			ChatAtlasSize,
			FVector2D(48.0f, 124.0f)),
		FVector2D(1019.0f, 343.0f),
		FVector2D(48.0f, 124.0f),
		30);

	PlaceOnCanvas(
		ChatContentCanvas,
		MakeSolidBorder(
			TEXT("ChatContactsPanel"),
			FLinearColor(0.045f, 0.055f, 0.06f, 0.98f)),
		FVector2D(6.0f, 66.0f),
		FVector2D(320.0f, 730.0f),
		2);
	PlaceOnCanvas(
		ChatContentCanvas,
		MakeSolidBorder(
			TEXT("ChatMessagesPanel"),
			FLinearColor(0.025f, 0.035f, 0.04f, 0.98f)),
		FVector2D(326.0f, 60.0f),
		FVector2D(695.0f, 750.0f),
		2);

	static const FText ChannelLabels[] = {
		// Unity shows Language 750011 for the world channel contact.
		NSLOCTEXT("RokChat", "WorldChannel", "王国"),
		NSLOCTEXT("RokChat", "AllianceChannel", "联盟"),
		NSLOCTEXT("RokChat", "PrivateChannel", "私聊")};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const bool bSelected = Index == SelectedChatChannel;
		const FVector2D RowSize(304.0f, 88.0f);
		UCanvasPanel* ChannelRow = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("ChatChannelRow_%d"), Index));
		PlaceOnCanvas(
			ChatContentCanvas,
			ChannelRow,
			FVector2D(11.0f, 82.0f + Index * 94.0f),
			RowSize,
			6);
		PlaceOnCanvas(
			ChannelRow,
			MakeSolidBorder(
				*FString::Printf(TEXT("ChatChannelShade_%d"), Index),
				bSelected
					? FLinearColor(0.12f, 0.15f, 0.16f, 0.98f)
					: FLinearColor(0.06f, 0.075f, 0.08f, 0.94f)),
			FVector2D::ZeroVector,
			RowSize,
			0);
		if (bSelected)
		{
			PlaceOnCanvas(
				ChannelRow,
				MakeAtlasBorder(
					*FString::Printf(TEXT("ChatChannelSelected_%d"), Index),
					ChatAtlasPath,
					ChatSelectedContactRect,
					ChatAtlasSize,
					RowSize,
					FMargin(0.16f)),
				FVector2D::ZeroVector,
				RowSize,
				1);
		}
		if (Index < 2)
		{
			PlaceOnCanvas(
				ChannelRow,
				MakeAtlasImage(
					*FString::Printf(TEXT("ChatChannelIcon_%d"), Index),
					ChatAtlasPath,
					Index == 0 ? ChatWorldIconRect : ChatAllianceIconRect,
					ChatAtlasSize,
					FVector2D(64.0f)),
				FVector2D(12.0f, 12.0f),
				FVector2D(64.0f),
				2);
		}
		else
		{
			PlaceOnCanvas(
				ChannelRow,
				MakeSolidBorder(
					TEXT("ChatPrivateIconShade"),
					FLinearColor(0.16f, 0.18f, 0.18f, 1.0f)),
				FVector2D(18.0f, 18.0f),
				FVector2D(52.0f),
				2);
			UTextBlock* PrivateGlyph = MakeScreenText(
				TEXT("ChatPrivateIconGlyph"),
				24,
				FLinearColor(0.95f, 0.86f, 0.55f, 1.0f),
				true,
				ETextJustify::Center);
			PrivateGlyph->SetText(NSLOCTEXT("RokChat", "PrivateGlyph", "私"));
			PlaceOnCanvas(
				ChannelRow,
				PrivateGlyph,
				FVector2D(18.0f, 25.0f),
				FVector2D(52.0f, 38.0f),
				3);
		}
		UTextBlock* ChannelLabel = MakeScreenText(
			*FString::Printf(TEXT("ChatChannelLabel_%d"), Index),
			23,
			bSelected
				? FLinearColor(1.0f, 0.88f, 0.48f, 1.0f)
				: FLinearColor(0.82f, 0.82f, 0.78f, 1.0f),
			true);
		ChannelLabel->SetText(ChannelLabels[Index]);
		PlaceOnCanvas(
			ChannelRow,
			ChannelLabel,
			FVector2D(94.0f, 25.0f),
			FVector2D(185.0f, 38.0f),
			3);
		PlaceOnCanvas(
			ChannelRow,
			MakeActionButton(
				*FString::Printf(TEXT("ChatChannel_%d"), Index),
				800 + Index,
				nullptr,
				FIntRect(),
				FIntPoint(1, 1),
				RowSize),
			FVector2D::ZeroVector,
			RowSize,
			10);
	}

	UTextBlock* ContactTitle = MakeScreenText(
		TEXT("ChatContactTitle"),
		20,
		FLinearColor(0.95f, 0.85f, 0.52f, 1.0f),
		true,
		ETextJustify::Center);
	ContactTitle->SetText(NSLOCTEXT("RokChat", "Contacts", "私聊联系人"));
	PlaceOnCanvas(
		ChatContentCanvas,
		ContactTitle,
		FVector2D(20.0f, 382.0f),
		FVector2D(292.0f, 36.0f),
		7);
	if (bChatTestFixtureState)
	{
		static const TCHAR* FixtureContacts[] = {
			TEXT("沙漠之鹰  在线"),
			TEXT("远征军  离线")};
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UTextBlock* Contact = MakeScreenText(
				*FString::Printf(TEXT("ChatContact_%d"), Index),
				18,
				Index == 0
					? FLinearColor(0.52f, 0.95f, 0.62f, 1.0f)
					: FLinearColor(0.68f, 0.68f, 0.66f, 1.0f));
			Contact->SetText(FText::FromString(FixtureContacts[Index]));
			PlaceOnCanvas(
				ChatContentCanvas,
				Contact,
				FVector2D(34.0f, 436.0f + Index * 58.0f),
				FVector2D(265.0f, 34.0f),
				7);
		}
	}
	else
	{
		UTextBlock* ContactsEmpty = MakeScreenText(
			TEXT("ChatContactsEmptyState"),
			17,
			FLinearColor(0.56f, 0.58f, 0.58f, 1.0f),
			false,
			ETextJustify::Center);
		ContactsEmpty->SetText(
			NSLOCTEXT("RokChat", "NoPrivateContacts", "暂无私聊联系人"));
		PlaceOnCanvas(
			ChatContentCanvas,
			ContactsEmpty,
			FVector2D(34.0f, 442.0f),
			FVector2D(264.0f, 32.0f),
			7);
	}

	PlaceOnCanvas(ChatContentCanvas,
		MakeActionButton(TEXT("ChatContactsToggleButton"), 812,
			ChatAtlasPath, ChatShrinkRect, ChatAtlasSize, FVector2D(32.0f)),
		FVector2D(334.65f, 71.61f), FVector2D(32.0f), 9);
	UTextBlock* ConversationTitle = MakeScreenText(
		TEXT("ChatConversationTitle"),
		24,
		FLinearColor(0.96f, 0.9f, 0.7f, 1.0f),
		true);
	ConversationTitle->SetText(ChannelLabels[SelectedChatChannel]);
	PlaceOnCanvas(
		ChatContentCanvas,
		ConversationTitle,
		FVector2D(438.0f, 72.0f),
		FVector2D(420.0f, 38.0f),
		7);
	if (SelectedChatChannel < 2)
	{
		PlaceOnCanvas(
			ChatContentCanvas,
			MakeAtlasImage(
				TEXT("ChatConversationChannelIcon"),
				ChatAtlasPath,
				SelectedChatChannel == 0
					? ChatWorldIconRect
					: ChatAllianceIconRect,
				ChatAtlasSize,
				FVector2D(48.0f)),
			FVector2D(378.0f, 65.0f),
			FVector2D(48.0f),
			7);
	}
	auto* MessageViewport = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ChatMessageViewport"));
	MessageViewport->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	const FVector2D MessageViewportSize(665.0f, 594.0f - EmojiLift);
	PlaceOnCanvas(ChatContentCanvas, MessageViewport, FVector2D(340, 121), MessageViewportSize, 4);
	PlaceOnCanvas(
		MessageViewport,
		MakeSolidBorder(
			TEXT("ChatMessageViewportBackground"),
			FLinearColor(0.012f, 0.018f, 0.021f, 0.94f)),
		FVector2D::ZeroVector,
		MessageViewportSize,
		4);

	// Unity sv_chat is a ScrollRect/ListView, not a fixed collection of
	// message cards. Keep its scroll offset when the emoji dock changes size.
	auto* MessageScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), TEXT("ChatMessageScroll"));
	MessageScroll->SetOrientation(Orient_Vertical);
	MessageScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	MessageScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	MessageScroll->SetScrollOffset(ChatMessageScrollOffset);
	PlaceOnCanvas(MessageViewport, MessageScroll, FVector2D::ZeroVector, MessageViewportSize, 5);
	auto* MessageExtent = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("ChatMessageExtent"));
	const float ContentHeight = bChatTestFixtureState ? FMath::Max(403.0f, float(MessageViewportSize.Y)) : float(MessageViewportSize.Y);
	MessageExtent->SetHeightOverride(ContentHeight);
	MessageScroll->AddChild(MessageExtent);
	auto* MessageContent = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ChatMessageContent"));
	MessageExtent->AddChild(MessageContent);
	if (bChatTestFixtureState)
	{
		static const TCHAR* WorldMessages[] = {
			TEXT("[王国] 沙漠之鹰：有人一起探索遗迹吗？"),
			TEXT("[王国] snow：坐标已分享 X:1146 Y:157"),
			TEXT("[系统] 欢迎来到第1王国。")};
		static const TCHAR* AllianceMessages[] = {
			TEXT("[联盟] 盟主：今晚20:00集结联盟要塞。"),
			TEXT("[联盟] snow：收到，我会提前到达。"),
			TEXT("[联盟] 盟友：资源点已标记。")};
		static const TCHAR* PrivateMessages[] = {
			TEXT("[好友] 沙漠之鹰：你的城市升级得怎么样？"),
			TEXT("[我] 正在升级市政厅。"),
			TEXT("[好友] 沙漠之鹰：需要资源就告诉我。")};
		const TCHAR* const* Messages = SelectedChatChannel == 0
			? WorldMessages
			: (SelectedChatChannel == 1
				? AllianceMessages
				: PrivateMessages);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			PlaceOnCanvas(
				MessageContent,
				MakeSolidBorder(
					*FString::Printf(TEXT("ChatMessageShade_%d"), Index),
					Index % 2 == 0
						? FLinearColor(0.07f, 0.085f, 0.09f, 0.96f)
						: FLinearColor(0.04f, 0.052f, 0.057f, 0.96f)),
				FVector2D(16.0f, 25.0f + Index * 126.0f),
				FVector2D(632.0f, 102.0f),
				5);
			UTextBlock* Message = MakeScreenText(
				*FString::Printf(TEXT("ChatMessage_%d"), Index),
				18,
				FLinearColor(0.9f, 0.92f, 0.9f, 1.0f));
			Message->SetText(FText::FromString(Messages[Index]));
			PlaceOnCanvas(
				MessageContent,
				Message,
				FVector2D(36.0f, 49.0f + Index * 126.0f),
				FVector2D(592.0f, 56.0f),
				6);
		}
	}
	else
	{
		UTextBlock* MessagesEmpty = MakeScreenText(
			TEXT("ChatMessagesEmptyState"),
			18,
			FLinearColor(0.52f, 0.54f, 0.54f, 1.0f),
			false,
			ETextJustify::Center);
		MessagesEmpty->SetText(
			NSLOCTEXT("RokChat", "NoMessages", "暂无消息"));
		PlaceOnCanvas(
			MessageContent,
			MessagesEmpty,
			FVector2D(140.0f, 253.0f - EmojiLift * 0.5f),
			FVector2D(384.0f, 38.0f),
			6);
	}

	if (bChatEmojiPanelOpen)
	{
		// Keep the dismiss button within the ScrollBox hierarchy so wheel
		// events bubble to the message list instead of leaking to the map.
		const FVector2D DismissSize(MessageViewportSize.X, ContentHeight);
		PlaceOnCanvas(MessageContent, MakeActionButton(TEXT("ChatEmojiDismissMask"),
			813, nullptr, FIntRect(), FIntPoint(1, 1), DismissSize),
			FVector2D::ZeroVector, DismissSize, 30);
	}
	auto* InputRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ChatInputRoot"));
	PlaceOnCanvas(ChatContentCanvas, InputRoot, FVector2D(0, -EmojiLift), FVector2D(1024, 810), 8);
	InputRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(
		InputRoot,
		MakeSolidBorder(
			TEXT("ChatInputShade"),
			FLinearColor(0.08f, 0.09f, 0.09f, 1.0f)),
		FVector2D(340.0f, 736.0f),
		FVector2D(520.0f, 48.0f),
		6);
	ChatInputTextBox = WidgetTree->ConstructWidget<UEditableTextBox>(
		UEditableTextBox::StaticClass(),
		TEXT("ChatInputTextBox"));
	ChatInputTextBox->SetText(FText::FromString(ChatDraft));
	ChatInputTextBox->SetHintText(
		NSLOCTEXT("RokChat", "InputHint", "输入消息…"));
	FEditableTextBoxStyle ChatInputStyle =
		FEditableTextBoxStyle::GetDefault();
	ChatInputStyle
		.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18))
		.SetForegroundColor(FLinearColor::White)
		.SetFocusedForegroundColor(FLinearColor::White)
		.SetBackgroundColor(FLinearColor(0.08f, 0.09f, 0.09f, 1.0f))
		.SetPadding(FMargin(16.0f, 8.0f));
	ChatInputTextBox->SetWidgetStyle(ChatInputStyle);
	ChatInputTextBox->SetClearKeyboardFocusOnCommit(false);
	ChatInputTextBox->OnTextCommitted.AddDynamic(
		this,
		&URokUnityFeatureScreenWidget::HandleChatInputCommitted);
	PlaceOnCanvas(
		InputRoot,
		ChatInputTextBox,
		FVector2D(344.0f, 737.0f),
		FVector2D(512.0f, 46.0f),
		7);
	PlaceOnCanvas(
		InputRoot,
		MakeActionButton(
			TEXT("ChatEmojiButton"),
			811,
			EmojiAtlasPath,
			EmojiFaceRect,
			EmojiAtlasSize,
			FVector2D(60.0f)),
		FVector2D(869.0f, 730.0f),
		FVector2D(60.0f),
		8);

	UCanvasPanel* SendVisual = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("ChatSendVisual"));
	PlaceOnCanvas(
		InputRoot,
		SendVisual,
		FVector2D(928.0f, 730.0f),
		FVector2D(60.0f),
		8);
	PlaceOnCanvas(
		SendVisual,
		MakeAtlasBorder(
			TEXT("ChatSendBackground"),
			CommonAtlasPath,
			MiniBlueButtonRect,
			CommonAtlasSize,
			FVector2D(60.0f),
			MiniBlueMargin),
		FVector2D::ZeroVector,
		FVector2D(60.0f),
		0);
	PlaceOnCanvas(
		SendVisual,
		MakeAtlasImage(
			TEXT("ChatSendIcon"),
			ChatAtlasPath,
			ChatSendRect,
			ChatAtlasSize,
			FVector2D(48.0f, 30.0f)),
		FVector2D(6.0f, 15.0f),
		FVector2D(48.0f, 30.0f),
		1);
	PlaceOnCanvas(
		SendVisual,
		MakeActionButton(
			TEXT("ChatSendButton"),
			810,
			nullptr,
			FIntRect(),
			FIntPoint(1, 1),
			FVector2D(60.0f)),
		FVector2D::ZeroVector,
		FVector2D(60.0f),
		10);

	if (bChatEmojiPanelOpen)
	{
		UCanvasPanel* EmojiPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			TEXT("ChatEmojiPanel"));
		PlaceOnCanvas(
			ChatContentCanvas,
			EmojiPanel,
			FVector2D(330.0f, 810.0f - EmojiHeight),
			FVector2D(694.0f, EmojiHeight),
			20);
		PlaceOnCanvas(
			EmojiPanel,
			MakeAtlasBorder(
				TEXT("ChatEmojiPanelBackground"),
				EmojiAtlasPath,
				EmojiPanelRect,
				EmojiAtlasSize,
				FVector2D(694.0f, EmojiHeight),
				FMargin(0.16f, 0.22f)),
			FVector2D::ZeroVector,
			FVector2D(694.0f, EmojiHeight),
			0);
		UTextBlock* EmojiTitle = MakeScreenText(
			TEXT("ChatEmojiPanelTitle"),
			20,
			FLinearColor(0.95f, 0.87f, 0.6f, 1.0f),
			true);
		EmojiTitle->SetText(NSLOCTEXT("RokChat", "EmojiTitle", "表情"));
		PlaceOnCanvas(
			EmojiPanel,
			EmojiTitle,
			FVector2D(24.0f, 18.0f),
			FVector2D(180.0f, 34.0f),
			2);
		PlaceOnCanvas(
			EmojiPanel,
			MakeActionButton(
				TEXT("ChatEmojiItem_0"),
				820,
				EmojiAtlasPath,
				EmojiTextRect,
				EmojiAtlasSize,
				FVector2D(88.0f)),
			FVector2D(28.0f, 72.0f),
			FVector2D(88.0f),
			3);
		PlaceOnCanvas(
			EmojiPanel,
			MakeActionButton(
				TEXT("ChatEmojiItem_1"),
				821,
				EmojiAtlasPath,
				EmojiFaceRect,
				EmojiAtlasSize,
				FVector2D(88.0f)),
			FVector2D(132.0f, 72.0f),
			FVector2D(88.0f),
			3);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok chat screen: channel=%d emoji_panel=%d fixture=%d authority=%s."),
		SelectedChatChannel,
		bChatEmojiPanelOpen ? 1 : 0,
		bChatTestFixtureState ? 1 : 0,
		bChatTestFixtureState ? TEXT("local_fixture") : TEXT("server"));
}

void URokUnityFeatureScreenWidget::BuildWorldPositionSearch()
{
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
		Description->SetText(FText::Format(
			NSLOCTEXT(
				"RokMapMarker",
				"MarkerDescription",
				"{0}  X:{1} Y:{2}"),
			FText::FromString(Marker.Description),
			FText::AsNumber(FMath::RoundToInt(Marker.DisplayCoordinate.X)),
			FText::AsNumber(FMath::RoundToInt(Marker.DisplayCoordinate.Y))));
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

	const FVector2D DockPosition(190.0f, 625.0f);
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
	static const float CellX[] = {1105.5f, 885.5f, 665.5f, 445.5f, 225.5f};
	for (int32 TypeIndex = 0; TypeIndex < UE_ARRAY_COUNT(TypeLabels); ++TypeIndex)
	{
		const FVector2D CellPosition(CellX[TypeIndex], 637.0f);
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
		PlaceOnCanvas(
			Cell,
			MakeAtlasImage(
				*FString::Printf(TEXT("ResourceSearchTypeIcon_%d"), TypeIndex),
				ResourceSearchAtlasPath,
				ResourceSearchTypeIconRects[TypeIndex],
				ResourceSearchAtlasSize,
				FVector2D(144.0f, 160.0f)),
			FVector2D(-3.5f, -2.3f),
			FVector2D(144.0f, 160.0f),
			1);
		UTextBlock* TypeLabel = MakeScreenText(
			*FString::Printf(TEXT("ResourceSearchTypeLabel_%d"), TypeIndex),
			18,
			FLinearColor::White,
			true,
			ETextJustify::Center);
		TypeLabel->SetText(TypeLabels[TypeIndex]);
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

	SelectedResourceSearchType = FMath::Clamp(SelectedResourceSearchType, 0, 4);
	const int32 SelectedLevel = SelectedResourceSearchType == 0
		? ResourceSearchBarbarianLevel
		: ResourceSearchResourceLevel;
	const int32 MaxLevel = SelectedResourceSearchType == 0 ? 25 : 6;
	const float PopupCenterX = 1160.0f - static_cast<float>(SelectedResourceSearchType) * 220.0f;
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
	Description->SetText(TypeDescriptions[SelectedResourceSearchType]);
	Description->SetAutoWrapText(true);
	PlaceOnCanvas(
		ModeCanvas,
		Description,
		FVector2D(PopupCenterX - 170.0f, 368.0f),
		FVector2D(340.0f, 56.0f),
		8);

	ResourceSearchLevelValueText = MakeScreenText(
		TEXT("ResourceSearchLevelValue"),
		20,
		FLinearColor(0.96f, 0.91f, 0.78f, 1.0f),
		true,
		ETextJustify::Center);
	ResourceSearchLevelValueText->SetText(FText::Format(
		NSLOCTEXT("RokResourceSearch", "LevelFormat", "等级:{0}"),
		FText::AsNumber(SelectedLevel)));
	PlaceOnCanvas(
		ModeCanvas,
		ResourceSearchLevelValueText,
		FVector2D(PopupCenterX - 63.62f, 439.1f),
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
	ResourceSearchSliderFill->SetBarFillType(EProgressBarFillType::RightToLeft);
	ResourceSearchSliderFill->SetPercent(
		MaxLevel > 1
			? static_cast<float>(SelectedLevel - 1) / static_cast<float>(MaxLevel - 1)
			: 0.0f);
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
	ResourceSearchSlider->SetRenderTransformPivot(FVector2D(0.5f));
	ResourceSearchSlider->SetRenderScale(FVector2D(-1.0f, 1.0f));
	ResourceSearchSlider->OnValueChanged.AddDynamic(
		this,
		&URokUnityFeatureScreenWidget::HandleResourceSearchSliderChanged);
	PlaceOnCanvas(
		ModeCanvas,
		ResourceSearchSlider,
		FVector2D(PopupCenterX - 115.0f, 466.5f),
		FVector2D(230.0f, 40.0f),
		11);

	const FVector2D PlusPosition(PopupCenterX - 188.05f, 454.2f);
	const FVector2D MinusPosition(PopupCenterX + 127.63f, 454.0f);
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

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("ResourceSearchSubmit"),
			1012,
			CommonAtlasPath,
			ResourceSearchSubmitRect,
			CommonAtlasSize,
			FVector2D(192.0f, 78.0f),
			NSLOCTEXT("RokResourceSearch", "Search", "搜索"),
			25,
			FLinearColor(0.23f, 0.16f, 0.04f, 1.0f)),
		FVector2D(PopupCenterX - 96.0f, 543.8f),
		FVector2D(192.0f, 78.0f),
		12);

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
	const FVector2D PopupRootPosition(PopupCenterX - 50, UnityReferenceHeight - 125);
	PlaceOnCanvas(ModeCanvas, ResourceSearchDockRoot, FVector2D::ZeroVector, FVector2D(UnityReferenceWidth, UnityReferenceHeight), 2);
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
			Position - (bDock ? FVector2D::ZeroVector : PopupRootPosition), Size, ZOrder);
	}
}

void URokUnityFeatureScreenWidget::BuildExpedition()
{
	ExpeditionPreviewRoot = nullptr;
	ExpeditionSideRoot = nullptr;
	ExpeditionCenterRoot = nullptr;
	ExpeditionCenterImg = nullptr;
	ExpeditionCenterFlagMes = nullptr;
	// This UE preview uses a 1440x810 local canvas; the child Unity prefab
	// does not serialize its outer CanvasScaler. CapturePoint is for legacy
	// 1440x900 captures and would incorrectly shift this header by -45.
	BuildSplitWindowFrame(FVector2D::ZeroVector, FVector2D(1440.0f, 810.0f),
		NSLOCTEXT("RokExpedition", "Title", "远征"), FVector2D(1357.0f, 4.0f));
	if (ExpeditionRows.Num() != 80)
	{
		UTextBlock* ErrorText = MakeScreenText(TEXT("ExpeditionSourceError"), 20,
			FLinearColor::White, false, ETextJustify::Center);
		ErrorText->SetText(FText::FromString(ExpeditionLoadError.IsEmpty()
			? TEXT("远征配置不可用") : ExpeditionLoadError));
		PlaceOnCanvas(ModeCanvas, ErrorText, FVector2D(240, 340),
			FVector2D(960, 80), 10);
		return;
	}

	FRokExpeditionStagePreviewRow Selected;
	if (!RokExpeditionStagePreviewData::FindById(ExpeditionRows,
		SelectedExpeditionStageId, Selected)) return;
	if (!bExpeditionTaskConfigVisible)
	{
	// Source sv_list_view is 1292 x 625. The source battleCoordinate pairs
	// position flags inside its vertically scrolling map; they are config,
	// not evidence of a player's cleared/unlocked stages.
	const FVector2D ViewSize(1292.0f, 625.0f);
	UScrollBox* MapScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), TEXT("ExpeditionMapScroll"));
	MapScroll->SetOrientation(Orient_Vertical);
	MapScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	MapScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	MapScroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	// UI_IF_ExpeditionFight/sv_list_view is centered 1292x625 and its
	// anchoredPosition.y=-16.5, hence top-left (74,109) on the 810 canvas.
	PlaceOnCanvas(ModeCanvas, MapScroll, FVector2D(74.0f, 109.0f), ViewSize, 2);
	USizeBox* MapExtent = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("ExpeditionMapExtent"));
	MapExtent->SetHeightOverride(2900.0f);
	MapExtent->SetWidthOverride(ViewSize.X);
	MapScroll->AddChild(MapExtent);
	UCanvasPanel* MapContent = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ExpeditionMapContent"));
	MapContent->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	MapExtent->AddChild(MapContent);

	UTextBlock* ConfigurationNotice = MakeScreenText(TEXT("ExpeditionConfigurationNotice"), 15,
		FLinearColor::White, false, ETextJustify::Center);
	ConfigurationNotice->SetText(NSLOCTEXT("RokExpedition", "ConfigOnly",
		"关卡配置预览 · 解锁与星级未同步"));
	PlaceOnCanvas(ModeCanvas, ConfigurationNotice, FVector2D(450.0f, 77.0f), FVector2D(540.0f, 28.0f), 5);

	for (const FRokExpeditionStagePreviewRow& Row : ExpeditionRows)
	{
		// UI_Item_ExpeditionMapLevel root is 126x100, centered on battleCoordinate.
		const FVector2D Position(float(Row.BattleCoordinate.X) - 63.0f,
			float(Row.BattleCoordinate.Y) - 50.0f);
		// Source Show() selects flag1/2/3/4 from account isUnlock. Unknown
		// account state cannot choose a valid visible flag or star reward box.
		// Retain the source atlas/slot for read-only inspection but hide it.
		const FIntRect FlagRect = SpriteRect(1459, 168, 61, 87);
		UImage* Flag = MakeAtlasImage(*FString::Printf(TEXT("ExpeditionStageFlag_%d"), Row.Id),
			ExpeditionBattleAtlasPath, FlagRect, FIntPoint(2048, 256), FVector2D(61, 87));
		PlaceOnCanvas(MapContent, Flag, Position + FVector2D(32.5f, -24.8f),
			FVector2D(61, 87), 1);
		Flag->SetVisibility(ESlateVisibility::Collapsed);
		UTextBlock* Label = MakeScreenText(
			*FString::Printf(TEXT("ExpeditionStageTitle_%d"), Row.Id), 19,
			FLinearColor::White, true, ETextJustify::Center);
		Label->SetText(FText::AsNumber(Row.Level));
		PlaceOnCanvas(MapContent, Label, Position + FVector2D(21, 19),
			FVector2D(84, 40), 2);
		PlaceOnCanvas(MapContent, MakeActionButton(
			*FString::Printf(TEXT("ExpeditionStageButton_%d"), Row.Id),
			24000 + Row.Id, nullptr, FIntRect(), FIntPoint::ZeroValue,
			FVector2D(126.0f, 100.0f)),
			Position, FVector2D(126.0f, 100.0f), 3);
	}

	// Source map flag click only selects a level and reveals its bottom panel.
	// EnterExpeditionMap requires the unimplemented battle scene and account
	// state, so this separately named local control opens config-only Task art.
	UCanvasPanel* MapSelection = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ExpeditionMapSelectedPanel"));
	MapSelection->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// pl_buttom is anchored to the canvas bottom at (20.4,61.3),
	// size (1150.3,131.9); these are its 1440x810 local canvas coordinates.
	PlaceOnCanvas(ModeCanvas, MapSelection, FVector2D(165.25f, 616.8f),
		FVector2D(1150.3f, 131.9f), 7);
	PlaceOnCanvas(MapSelection, MakeSolidBorder(TEXT("ExpeditionMapSelectedShade"),
		FLinearColor(0.06f, 0.09f, 0.11f, 0.91f)), FVector2D::ZeroVector,
		FVector2D(1150.3f, 131.9f), 0);
	UTextBlock* MapLevel = MakeScreenText(TEXT("ExpeditionSelectedLevel"), 22,
		FLinearColor::White, true, ETextJustify::Center);
	MapLevel->SetText(FText::Format(NSLOCTEXT("RokExpedition", "SelectedConfig", "第 {0} 关 · 进度未同步"),
		FText::AsNumber(Selected.Level)));
	PlaceOnCanvas(MapSelection, MapLevel, FVector2D(936.7f, 22.0f), FVector2D(199.5f, 80.0f), 1);
	PlaceOnCanvas(MapSelection, MakeActionButton(TEXT("ExpeditionTaskConfigOpenButton"),
		1123, CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize,
		FVector2D(204, 60), NSLOCTEXT("RokExpedition", "OpenConfig", "查看关卡配置"), 19,
		FLinearColor(0.25f, 0.13f, 0.02f, 1.0f), ESlateBrushDrawType::Box,
		FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		FVector2D(454.0f, 36.0f), FVector2D(204, 60), 2);
	PlaceOnCanvas(MapSelection, MakeActionButton(TEXT("ExpeditionChallenge"), 1120,
		CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize,
		FVector2D(202, 84), NSLOCTEXT("RokExpedition", "Challenge", "挑战"), 20,
		FLinearColor(0.25f, 0.13f, 0.02f, 1.0f), ESlateBrushDrawType::Box,
		FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		FVector2D(31.15f, 30.95f), FVector2D(202, 84), 2);
	ExpeditionAnimationPhase = FName(TEXT("Idle"));
	ExpeditionAnimationElapsed = 0.0f;
	return;
	}

	// Task is a separate source full-screen UI. The local control above is
	// explicitly read-only and does not send EnterExpeditionMap or Start.
	UTextBlock* TaskConfigNotice = MakeScreenText(TEXT("ExpeditionConfigurationNotice"), 15,
		FLinearColor::White, false, ETextJustify::Center);
	TaskConfigNotice->SetText(NSLOCTEXT("RokExpedition", "LocalTaskConfigOnly",
		"关卡配置浏览 · 未进入远征战斗场景 · 账号进度未同步"));
	PlaceOnCanvas(ModeCanvas, TaskConfigNotice, FVector2D(430, 60),
		FVector2D(580, 28), 9);
	UCanvasPanel* Side = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ExpeditionSideRoot"));
	Side->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// pl_side anchors to the right and stretches vertically with -32.7 height.
	PlaceOnCanvas(ModeCanvas, Side, FVector2D(1294.0f, 12.25f),
		FVector2D(146.0f, 777.3f), 6);
	ExpeditionSideRoot = Side;
	PlaceOnCanvas(Side, MakeSolidBorder(TEXT("ExpeditionSideShade"),
		FLinearColor(0.08f, 0.1f, 0.1f, 0.88f)), FVector2D::ZeroVector,
		FVector2D(146.0f, 777.3f), 0);
	UTextBlock* StageLevel = MakeScreenText(TEXT("ExpeditionSelectedLevel"), 25,
		FLinearColor::White, true, ETextJustify::Center);
	StageLevel->SetText(FText::AsNumber(Selected.Level));
	PlaceOnCanvas(Side, StageLevel, FVector2D(12, 34), FVector2D(122, 42), 2);
	UTextBlock* State = MakeScreenText(TEXT("ExpeditionAccountState"), 15,
		FLinearColor(0.85f, 0.82f, 0.67f), false, ETextJustify::Center);
	State->SetText(NSLOCTEXT("RokExpedition", "UnknownAccount", "进度未同步"));
	PlaceOnCanvas(Side, State, FVector2D(4, 90), FVector2D(138, 36), 2);

	UCanvasPanel* Center = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ExpeditionCenterRoot"));
	Center->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Center->SetRenderTransformPivot(FVector2D(0.5f));
	// UI_IF_ExpeditionFightTask/pl_center stretches over the outer canvas.
	// The child prefab itself does not serialize a CanvasScaler reference;
	// 1440x810 is this UE preview canvas, not a claimed Unity GameView sample.
	PlaceOnCanvas(ModeCanvas, Center, FVector2D::ZeroVector,
		FVector2D(1440.0f, 810.0f), 7);
	ExpeditionCenterRoot = Center;
	// Source bg belongs to pl_center/pl_flagMes, whereas the Show-scaled img
	// is its sibling. The red cloth and condition copy therefore share the
	// FlagMes alpha/Y animation, and never inherit the header img scale.
	UCanvasPanel* FlagMes = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ExpeditionCenterFlagMes"));
	FlagMes->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(Center, FlagMes, FVector2D::ZeroVector, FVector2D(1440.0f, 810.0f), 0);
	ExpeditionCenterFlagMes = FlagMes;
	PlaceOnCanvas(FlagMes, MakeAtlasImage(TEXT("ExpeditionTaskPanelImage"),
		ExpeditionTaskPanelPath, SpriteRect(0, 0, 676, 761), FIntPoint(676, 761),
		FVector2D(676.0f, 761.0f)), FVector2D(382.0f, 47.5f),
		FVector2D(676.0f, 761.0f), 0);
	UCanvasPanel* CenterImg = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ExpeditionCenterImg"));
	CenterImg->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	CenterImg->SetRenderTransformPivot(FVector2D(0.5f));
	PlaceOnCanvas(Center, CenterImg, FVector2D(266.0f, 93.0f),
		FVector2D(908.0f, 102.0f), 1);
	ExpeditionCenterImg = CenterImg;
	// pl_center/img's own pl_com_1009 Image is disabled in the prefab. Its two
	// enabled ui_result children use the same sprite, left mirrored in X.
	UImage* LeftFrame = MakeAtlasImage(TEXT("ExpeditionCenterFrameLeft"),
		ExpeditionResultAtlasPath, SpriteRect(1, 131, 454, 138),
		FIntPoint(512, 512), FVector2D(454.0f, 138.0f));
	LeftFrame->SetRenderTransformPivot(FVector2D(0.5f));
	LeftFrame->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(CenterImg, LeftFrame, FVector2D(0.0f, -18.0f),
		FVector2D(454.0f, 138.0f), 0);
	PlaceOnCanvas(CenterImg, MakeAtlasImage(TEXT("ExpeditionCenterFrameRight"),
		ExpeditionResultAtlasPath, SpriteRect(1, 131, 454, 138),
		FIntPoint(512, 512), FVector2D(454.0f, 138.0f)),
		FVector2D(454.0f, -18.0f), FVector2D(454.0f, 138.0f), 0);
	UTextBlock* Detail = MakeScreenText(TEXT("ExpeditionDetailTitle"), 23,
		FLinearColor::White, true, ETextJustify::Center);
	Detail->SetText(FText::Format(NSLOCTEXT("RokExpedition", "Stage", "第 {0} 关"),
		FText::AsNumber(Selected.Level)));
	PlaceOnCanvas(Center, Detail, FVector2D(500, 106), FVector2D(440, 42), 2);
	for (int32 Index = 0; Index < Selected.StarDescriptions.Num(); ++Index)
	{
		UTextBlock* Condition = MakeScreenText(
			*FString::Printf(TEXT("ExpeditionStarCondition_%d"), Index), 17,
			FLinearColor(0.88f, 0.87f, 0.79f), false, ETextJustify::Left);
		Condition->SetText(FText::FromString(Selected.StarDescriptions[Index]));
		PlaceOnCanvas(FlagMes, Condition, FVector2D(502, 255 + Index * 38),
			FVector2D(436, 36), 2);
	}
	UTextBlock* Troops = MakeScreenText(TEXT("ExpeditionTroopCount"), 17,
		FLinearColor::White, false, ETextJustify::Center);
	Troops->SetText(FText::Format(
		NSLOCTEXT("RokExpedition", "Troops", "队伍配置 {0} · 尚未同步"),
		FText::AsNumber(Selected.TroopsNumber)));
	// Task content and btn_start1 are descendants of pl_flagMes in the
	// source prefab. Keep their screen slots, but inherit its alpha/Y curve.
	PlaceOnCanvas(FlagMes, Troops, FVector2D(500, 455), FVector2D(440, 38), 2);
	PlaceOnCanvas(FlagMes, MakeActionButton(TEXT("ExpeditionChallenge"), 1120,
		CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize,
		FVector2D(191.66f, 66.98f), NSLOCTEXT("RokExpedition", "Challenge", "挑战"),
		25, FLinearColor(0.25f, 0.13f, 0.02f, 1.0f), ESlateBrushDrawType::Box,
		FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		FVector2D(624.17f, 600.51f), FVector2D(191.66f, 66.98f), 4);

	UCanvasPanel* Preview = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("ExpeditionPreviewRoot"));
	Preview->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// pl_preview also stretches over the full source task root. Its present
	// neutral inner shade is a clearly partial content placeholder only.
	PlaceOnCanvas(ModeCanvas, Preview, FVector2D::ZeroVector,
		FVector2D(1440.0f, 810.0f), 8);
	ExpeditionPreviewRoot = Preview;
	// The sealed firstBg sprite is only a small reward strip in the main prefab;
	// it is not a full preview panel. Keep this partial backdrop until its own
	// source panel chain is extracted instead of stretching that strip.
	PlaceOnCanvas(Preview, MakeSolidBorder(TEXT("ExpeditionPreviewShade"),
		FLinearColor(0.06f, 0.1f, 0.14f, 0.94f)), FVector2D(822.0f, 310.0f),
		FVector2D(390.0f, 448.0f), 0);
	UTextBlock* PreviewTitle = MakeScreenText(TEXT("ExpeditionPreviewTitle"), 23,
		FLinearColor::White, true, ETextJustify::Center);
	PreviewTitle->SetText(FText::Format(
		NSLOCTEXT("RokExpedition", "EnemyPreview", "第 {0} 关 · 战前预览"),
		FText::AsNumber(Selected.Level)));
	PlaceOnCanvas(Preview, PreviewTitle, FVector2D(842, 332), FVector2D(350, 44), 2);
	UTextBlock* PreviewState = MakeScreenText(TEXT("ExpeditionPreviewState"), 17,
		FLinearColor::White, false, ETextJustify::Center);
	PreviewState->SetText(NSLOCTEXT("RokExpedition", "EnemyState",
		"敌军与统帅资料需要战斗配置和账号状态"));
	PlaceOnCanvas(Preview, PreviewState, FVector2D(840, 452), FVector2D(354, 74), 2);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("ExpeditionPreviewButton"),
		1121, CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize,
		FVector2D(170, 56), NSLOCTEXT("RokExpedition", "Preview", "预览"), 21,
		FLinearColor(0.25f, 0.13f, 0.02f, 1), ESlateBrushDrawType::Box,
		FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		FVector2D(1010, 226), FVector2D(170, 56), 10);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("ExpeditionBackButton"),
		1122, CommonAtlasPath, CommanderBackRect, CommonAtlasSize,
		FVector2D(69)), FVector2D(116, 70), FVector2D(69), 12);
	ExpeditionAnimationPhase = FName(TEXT("NormalTransition"));
	ExpeditionAnimationElapsed = 0.0f;
	AdvanceExpeditionAnimation(0.0f);
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
		TaskChallenge->SetIsEnabled(!bToPreview);
	}
	if (Time >= Duration) ExpeditionAnimationPhase = FName(TEXT("Idle"));
}

void URokUnityFeatureScreenWidget::BuildGemShop()
{
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
		Amount->SetText(FText::Format(
			NSLOCTEXT("RokGemShop", "GemAmount", "{0} 宝石"),
			FText::AsNumber(Package.DenarNum)));
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
		Bonus->SetText(FText::Format(
			NSLOCTEXT(
				"RokGemShop",
				"BonusConfigFormat",
				"首充赠 {0}\n常规赠 {1}"),
			FText::AsNumber(Package.FirstPresenter),
			FText::AsNumber(Package.Presenter)));
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
		Product->SetText(FText::Format(
			NSLOCTEXT(
				"RokGemShop",
				"ProductConfigFormat",
				"商品配置 {0}（不是现金价格）"),
			FText::AsNumber(Package.ProductId)));
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

void URokUnityFeatureScreenWidget::BuildCharge()
{
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

	FRokUnityEconomyChargePage SelectedPage;
	const bool bHasPageConfig = Catalog
		&& Catalog->FindChargePageByPagingType(
			SelectedChargePageType,
			SelectedPage);
	UTextBlock* SourceState = MakeScreenText(
		TEXT("ChargeCatalogState"),
		15,
		FLinearColor(0.67f, 0.65f, 0.67f, 1.0f));
	SourceState->SetText(
		bHasPageConfig
			? FText::Format(
				NSLOCTEXT(
					"RokCharge",
					"CatalogSourceFormat",
					"Unity RechargeList · 行 {0} · ID {1} · 类型 {2}"),
				FText::AsNumber(SelectedPage.SourceRow),
				FText::AsNumber(SelectedPage.Id),
				FText::AsNumber(SelectedPage.PagingType))
			: NSLOCTEXT(
				"RokCharge",
				"CatalogUnavailable",
				"Unity 充值目录尚未加载；当前仅保留页面结构。"));
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
			Amount->SetText(FText::Format(
				NSLOCTEXT("RokCharge", "GemAmount", "{0} 宝石"),
				FText::AsNumber(Package.DenarNum)));
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
			Bonus->SetText(FText::Format(
				NSLOCTEXT(
					"RokCharge",
					"BonusConfigFormat",
					"首充赠 {0}\n常规赠 {1}"),
				FText::AsNumber(Package.FirstPresenter),
				FText::AsNumber(Package.Presenter)));
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
			Product->SetText(FText::Format(
				NSLOCTEXT(
					"RokCharge",
					"ProductConfigFormat",
					"商品配置 {0}（不是现金价格）"),
				FText::AsNumber(Package.ProductId)));
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
		PageBoundary->SetText(FText::Format(
			NSLOCTEXT(
				"RokCharge",
				"PageBoundaryFormat",
				"{0} 的页面入口与 Unity 配置已接入。\n活动进度、限购、领奖资格和商品内容仍需服务器状态。"),
			ResolveChargePageLabel(SelectedChargePageType)));
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

void URokUnityFeatureScreenWidget::BuildBuildCity()
{
	// UI_Item_BuildCityBuilding uses Unity pixel sizes. Slate renders points
	// at 96 DPI; preserve fractional 13.5pt for the source 18px body text.
	auto ApplyCardFont = [](UTextBlock* Text, float Pixels)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Pixels * 72.0f / 96.0f;
		Text->SetFont(Font);
		Text->SetShadowOffset(FVector2D::ZeroVector);
	};
	BuildCityAnimatedCards.Reset();
	BuildCityCardSourceIndices.Reset();
	VisibleBuildCityTypeIds.Reset();
	BuildableBuildCityTypeIds.Reset();
	URokUnityGameplayCatalogSubsystem* Catalog = nullptr;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		Catalog =
			GameInstance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
		if (Catalog && !Catalog->IsCatalogLoaded())
		{
			Catalog->LoadCatalog();
		}
	}

	int32 TownHallLevel = 1;
	TMap<int32, int32> CurrentBuildingCounts;
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>()
		: nullptr;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			AActor* Actor = *ActorIt;
			int32 TaggedTypeId = 0;
			for (const FName& Tag : Actor->Tags)
			{
				const FString TagString = Tag.ToString();
				if (TagString.StartsWith(TEXT("UnityBuildingType_")))
				{
					TaggedTypeId = FCString::Atoi(
						*TagString.Mid(18));
					break;
				}
			}
			if (TaggedTypeId <= 0)
			{
				continue;
			}
			CurrentBuildingCounts.FindOrAdd(TaggedTypeId) += 1;
			if (TaggedTypeId == 1 && UpgradeSubsystem)
			{
				TownHallLevel = FMath::Max(
					TownHallLevel,
					UpgradeSubsystem->GetBuildingStateForActor(Actor).Level);
			}
		}
	}

	auto ResolveBuildState = [&CurrentBuildingCounts, TownHallLevel](
		const FRokUnityCatalogBuildingType& BuildingType,
		int32& OutCurrentCount,
		int32& OutCurrentLimit,
		int32& OutMaximumLimit,
		int32& OutUnlockTownHallLevel)
	{
		OutCurrentCount = CurrentBuildingCounts.FindRef(BuildingType.TypeId);
		OutCurrentLimit = BuildingType.GetCountLimit(TownHallLevel);
		OutMaximumLimit = BuildingType.GetMaximumCountLimit();
		OutUnlockTownHallLevel = 0;
		for (const TPair<int32, int32>& Pair :
			BuildingType.CountLimitByTownHallLevel)
		{
			if (Pair.Key > TownHallLevel
				&& Pair.Value > OutCurrentCount
				&& (OutUnlockTownHallLevel == 0
					|| Pair.Key < OutUnlockTownHallLevel))
			{
				OutUnlockTownHallLevel = Pair.Key;
			}
		}

		if (OutMaximumLimit <= 0)
		{
			return BuildingType.Group == 3 ? 0 : 1;
		}
		if (OutCurrentCount < OutCurrentLimit)
		{
			return 0;
		}
		return OutCurrentCount >= OutMaximumLimit ? 2 : 1;
	};

	int32 CategoryBuildableCounts[3] = {0, 0, 0};
	if (Catalog && Catalog->IsCatalogLoaded())
	{
		for (const int32 TypeId : Catalog->GetBuildingTypeIds())
		{
			FRokUnityCatalogBuildingType BuildingType;
			if (!Catalog->GetBuildingType(TypeId, BuildingType)
				|| BuildingType.Group < 1
				|| BuildingType.Group > 3
				|| BuildingType.LevelCount <= 0)
			{
				continue;
			}
			int32 CurrentCount = 0;
			int32 CurrentLimit = 0;
			int32 MaximumLimit = 0;
			int32 UnlockTownHallLevel = 0;
			if (ResolveBuildState(
					BuildingType,
					CurrentCount,
					CurrentLimit,
					MaximumLimit,
					UnlockTownHallLevel)
				== 0)
			{
				CategoryBuildableCounts[BuildingType.Group - 1] += 1;
			}
		}
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("BuildCityMaskClose"),
			1,
			nullptr,
			FIntRect(),
			FIntPoint::ZeroValue,
			FVector2D(1440.0f, 810.0f)),
		FVector2D::ZeroVector,
		FVector2D(1440.0f, 810.0f),
		0);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(
			TEXT("BuildCityDrawerBackground"),
			BuildCityLoadingAtlasPath,
			BuildCityDrawerRect,
			BuildCityLoadingAtlasSize,
			// rect/bg: bottom anchored, x=15, y=-10, delta=(22,153), pivot=(1,0).
			// Sprite border is Unity (left,bottom,right,top), not Slate order.
			FVector2D(1462.0f, 153.0f),
			FMargin(0.47f, 0.41f, 0.46f, 0.44f)),
		CapturePoint(-7.0f, 712.0f),
		FVector2D(1462.0f, 153.0f),
		2);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("BuildCityDrawerInputShield"),
			1399,
			nullptr,
			FIntRect(),
			FIntPoint::ZeroValue,
			FVector2D(1440.0f, 527.2f)),
		CapturePoint(0.0f, 327.8f),
		FVector2D(1440.0f, 527.2f),
		3);

	static const int32 EconomyTypes[] = {
		2, 3, 4, 5, 7, 13, 15, 16, 19, 20, 21, 30};
	static const int32 MilitaryTypes[] = {
		9, 10, 11, 12, 14, 17, 18, 28, 29, 31};
	static const int32 DecorationTypes[] = {51, 52, 53};
	const int32* OrderedTypes = EconomyTypes;
	int32 OrderedTypeCount = UE_ARRAY_COUNT(EconomyTypes);
	if (SelectedBuildCityCategory == 1)
	{
		OrderedTypes = MilitaryTypes;
		OrderedTypeCount = UE_ARRAY_COUNT(MilitaryTypes);
	}
	else if (SelectedBuildCityCategory == 2)
	{
		OrderedTypes = DecorationTypes;
		OrderedTypeCount = UE_ARRAY_COUNT(DecorationTypes);
	}
	for (int32 Index = 0; Index < OrderedTypeCount; ++Index)
	{
		FRokUnityCatalogBuildingType BuildingType;
		if (Catalog
			&& Catalog->GetBuildingType(OrderedTypes[Index], BuildingType)
			&& BuildingType.LevelCount > 0)
		{
			VisibleBuildCityTypeIds.Add(OrderedTypes[Index]);
		}
	}

	UScrollBox* BuildingScroll =
		WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(),
			TEXT("BuildCityBuildingScroll"));
	BuildingScroll->SetOrientation(Orient_Horizontal);
	BuildingScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	BuildingScroll->SetAllowOverscroll(true);
	BuildingScroll->SetAnimateWheelScrolling(true);
	BuildingScroll->SetWheelScrollMultiplier(9.0f);
	// UI_Win_BuildCity: n is the mask, stretched 241.1 above listRect;
	// its child v is only the 498px layout viewport and has no mask.
	// Keep the scroll layout unchanged while allowing Show's 1.2x overshoot.
	UCanvasPanel* BuildingMask = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("BuildCityListMask"));
	BuildingMask->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BuildingMask->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(
		ModeCanvas,
		BuildingMask,
		CapturePoint(0.0f, 366.9f - 241.1f),
		FVector2D(1295.0f, 498.1f + 241.1f),
		4);
	BuildingScroll->SetClipping(EWidgetClipping::Inherit);
	PlaceOnCanvas(BuildingMask, BuildingScroll, FVector2D(0.0f, 241.1f),
		FVector2D(1295.0f, 498.1f), 0);
	const float BuildCityCardStride = 332.0f;
	const float BuildCityListWidth = 1295.0f;
	const float RightToLeftLeadingSpace = FMath::Max(
		0.0f,
		BuildCityListWidth
			- VisibleBuildCityTypeIds.Num() * BuildCityCardStride);
	if (!VisibleBuildCityTypeIds.IsEmpty()
		&& RightToLeftLeadingSpace > KINDA_SMALL_NUMBER)
	{
		USizeBox* AlignmentSpacer =
			WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(),
				TEXT("BuildCityRightAlignmentSpacer"));
		AlignmentSpacer->SetWidthOverride(RightToLeftLeadingSpace);
		AlignmentSpacer->SetHeightOverride(1.0f);
		BuildingScroll->AddChild(AlignmentSpacer);
	}

	if (VisibleBuildCityTypeIds.IsEmpty())
	{
		UTextBlock* Empty = MakeScreenText(
			TEXT("BuildCityCatalogState"),
			23,
			FLinearColor(0.78f, 0.76f, 0.68f, 1.0f),
			false,
			ETextJustify::Center);
		Empty->SetText(
			Catalog
				? NSLOCTEXT(
					"RokBuildCity",
					"CategoryEmpty",
					"该分类没有可建造的 Unity 配置")
				: NSLOCTEXT(
					"RokBuildCity",
					"CatalogUnavailable",
					"Unity 建筑配置运行时尚未加载"));
		PlaceOnCanvas(
			ModeCanvas,
			Empty,
			CapturePoint(330.0f, 580.0f),
			FVector2D(780.0f, 50.0f),
			5);
	}
	else
	{
		for (int32 Index = VisibleBuildCityTypeIds.Num() - 1;
			Index >= 0;
			--Index)
		{
			const int32 TypeId = VisibleBuildCityTypeIds[Index];
			FRokUnityCatalogBuildingType BuildingType;
			if (!Catalog->GetBuildingType(TypeId, BuildingType))
			{
				continue;
			}

			int32 CurrentCount = 0;
			int32 CurrentLimit = 0;
			int32 MaximumLimit = 0;
			int32 UnlockTownHallLevel = 0;
			const int32 BuildState = ResolveBuildState(
				BuildingType,
				CurrentCount,
				CurrentLimit,
				MaximumLimit,
				UnlockTownHallLevel);
			const bool bBuildable = BuildState == 0;
			if (bBuildable)
			{
				BuildableBuildCityTypeIds.Add(TypeId);
			}

			UCanvasPanel* Card =
				WidgetTree->ConstructWidget<UCanvasPanel>(
					UCanvasPanel::StaticClass(),
					*FString::Printf(
						TEXT("BuildCityCard_%d"),
						TypeId));
			Card->SetRenderTransformPivot(FVector2D(0.5));
			BuildCityAnimatedCards.Add(Card);
			BuildCityCardSourceIndices.Add(Index);
			const FIntRect CardRect = bBuildable
				? BuildCityCardLightRect
				: BuildCityCardDarkRect;
			PlaceOnCanvas(
				Card,
				MakeAtlasImage(
					*FString::Printf(TEXT("BuildCityCardLeft_%d"), TypeId),
					BuildCityImageAtlasPath,
					CardRect,
					BuildCityImageAtlasSize,
					FVector2D(159.0f, 501.0f)),
				FVector2D(7.0f, 0.2f),
				FVector2D(159.0f, 501.0f));
			UImage* CardRight = MakeAtlasImage(
				*FString::Printf(TEXT("BuildCityCardRight_%d"), TypeId),
				BuildCityImageAtlasPath,
				CardRect,
				BuildCityImageAtlasSize,
				FVector2D(159.0f, 501.0f));
			CardRight->SetRenderTransformPivot(FVector2D(0.5f));
			CardRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
			PlaceOnCanvas(
				Card,
				CardRight,
				FVector2D(166.0f, 0.2f),
				FVector2D(159.0f, 501.0f));

			const FString PreviewPath =
				ResolveBuildCityPreviewPath(TypeId);
			const FIntPoint PreviewSourceSize =
				ResolveBuildCityPreviewSize(TypeId);
			const float PreviewWidth = 294.0f;
			const float PreviewHeight = PreviewWidth
				* static_cast<float>(PreviewSourceSize.Y)
				/ static_cast<float>(PreviewSourceSize.X);
			UImage* Preview = MakeAtlasImage(
				*FString::Printf(TEXT("BuildCityCardPreview_%d"), TypeId),
				*PreviewPath,
				SpriteRect(
					0,
					0,
					PreviewSourceSize.X,
					PreviewSourceSize.Y),
				PreviewSourceSize,
				FVector2D(PreviewWidth, PreviewHeight));
			Preview->SetColorAndOpacity(
				bBuildable
					? FLinearColor::White
					: FLinearColor(0.42f, 0.42f, 0.42f, 0.88f));
			PlaceOnCanvas(
				Card,
				Preview,
				FVector2D(
					19.0f,
					FMath::Max(-24.0f, 138.0f - PreviewHeight * 0.5f)),
				FVector2D(PreviewWidth, PreviewHeight),
				2);

			UTextBlock* Name = MakeScreenText(
				*FString::Printf(TEXT("BuildCityCardName_%d"), TypeId),
				23,
				bBuildable ? FLinearColor::Black : FLinearColor::White,
				true,
				ETextJustify::Center);
			ApplyCardFont(Name, 24);
			Name->SetText(
				BuildingType.DisplayNameZhCn.IsEmpty()
					? ResolveBuildingTypeName(TypeId)
					: FText::FromString(BuildingType.DisplayNameZhCn));
			PlaceOnCanvas(
				Card,
				Name,
				FVector2D(18.75f, 202.5f),
				FVector2D(294.5f, 40.0f),
				4);

			UTextBlock* Description = MakeScreenText(
				*FString::Printf(
					TEXT("BuildCityCardDescription_%d"),
					TypeId),
				16,
				bBuildable ? FLinearColor::Black : FLinearColor::White,
				false,
				ETextJustify::Center);
			ApplyCardFont(Description, 18);
			Description->SetText(
				ResolveBuildCityDescription(BuildingType));
			Description->SetAutoWrapText(true);
			PlaceOnCanvas(
				Card,
				Description,
				FVector2D(18.75f, 282.5f),
				FVector2D(294.5f, 67.6f),
				4);

			PlaceOnCanvas(
				Card,
				MakeAtlasBorder(
					*FString::Printf(TEXT("BuildCityCardState_%d"), TypeId),
					CommonAtlasPath,
					TaskRowRect,
					CommonAtlasSize,
					bBuildable
						? FVector2D(270.0f, 41.9f)
						: FVector2D(270.0f, 116.4f),
					FMargin(
						23.0f / 74.0f,
						22.0f / 67.0f,
						21.0f / 74.0f,
						13.0f / 67.0f)),
				bBuildable
					? FVector2D(31.0f, 430.5f)
					: FVector2D(31.0f, 347.2f),
				bBuildable
					? FVector2D(270.0f, 41.9f)
					: FVector2D(270.0f, 116.4f),
				5);

			if (bBuildable)
			{
				struct FBuildResourceEntry
				{
					FIntRect IconRect;
					int32 Amount = 0;
				};
				TArray<FBuildResourceEntry> Resources;
				if (BuildingType.Level1Cost.Food > 0)
				{
					Resources.Add({FoodRect, BuildingType.Level1Cost.Food});
				}
				if (BuildingType.Level1Cost.Wood > 0)
				{
					Resources.Add({WoodRect, BuildingType.Level1Cost.Wood});
				}
				if (BuildingType.Level1Cost.Stone > 0)
				{
					Resources.Add({StoneRect, BuildingType.Level1Cost.Stone});
				}
				if (BuildingType.Level1Cost.Gold > 0)
				{
					Resources.Add({GoldRect, BuildingType.Level1Cost.Gold});
				}
				for (int32 ResourceIndex = 0;
					ResourceIndex < FMath::Min(2, Resources.Num());
					++ResourceIndex)
				{
					const float ResourceX =
						70.0f + ResourceIndex * 110.0f;
					PlaceOnCanvas(
						Card,
						MakeAtlasImage(
							*FString::Printf(
								TEXT("BuildCityCardResourceIcon_%d_%d"),
								TypeId,
								ResourceIndex),
							CommonAtlasPath,
							Resources[ResourceIndex].IconRect,
							CommonAtlasSize,
							FVector2D(42.0f)),
						FVector2D(ResourceX, 354.0f),
						FVector2D(42.0f),
						7);
					UTextBlock* Amount = MakeScreenText(
						*FString::Printf(
							TEXT("BuildCityCardResourceAmount_%d_%d"),
							TypeId,
							ResourceIndex),
						15,
						FLinearColor(0.20f, 0.17f, 0.12f, 1.0f),
						true);
					Amount->SetText(
						FText::AsNumber(Resources[ResourceIndex].Amount));
					PlaceOnCanvas(
						Card,
						Amount,
						FVector2D(ResourceX + 42.0f, 362.0f),
						FVector2D(65.0f, 24.0f),
						8);
				}

				// UI_Item_BuildCityBuilding/pl_claim/time/titleCount,titleTime.
				// Language 180304/180303; source bottom-aligned titles above values.
				for (int32 TitleIndex = 0; TitleIndex < 2; ++TitleIndex)
				{
					const bool bTimeTitle = TitleIndex == 1;
					const FName TitleName(*FString::Printf(TEXT("BuildCityCard%sTitle_%d"), bTimeTitle ? TEXT("Duration") : TEXT("Count"), TypeId));
					UTextBlock* Title = MakeScreenText(TitleName, 14,
						FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1), false,
						bTimeTitle ? ETextJustify::Right : ETextJustify::Left);
					ApplyCardFont(Title, 18);
					Title->SetText(bTimeTitle ? NSLOCTEXT("RokBuildCity", "Language180303", "建造时间")
						: NSLOCTEXT("RokBuildCity", "Language180304", "已建造"));
					const FVector2D TitleSize(bTimeTitle ? 155.72f : 136.9f, 38.4f);
					USizeBox* TitleBox = WrapFixedSize(Title,
						*FString::Printf(TEXT("%sLayout"), *TitleName.ToString()), TitleSize);
					CastChecked<USizeBoxSlot>(Title->Slot)->SetVerticalAlignment(VAlign_Bottom);
					PlaceOnCanvas(Card, TitleBox, FVector2D(bTimeTitle ? 139.24f : 35.75f, 387.1f), TitleSize, 8);
				}
				UTextBlock* Count = MakeScreenText(
					*FString::Printf(TEXT("BuildCityCardCount_%d"), TypeId),
					17,
					FLinearColor(0.8509804f, 0.83137256f, 0.7294118f, 1),
					false,
					ETextJustify::Center);
				ApplyCardFont(Count, 18);
				Count->SetText(FText::Format(
					NSLOCTEXT("RokBuildCity", "CountFormat", "{0}/{1}"),
					FText::AsNumber(CurrentCount),
					FText::AsNumber(CurrentLimit)));
				PlaceOnCanvas(
					Card,
					Count,
					FVector2D(31.0f, 430.0f),
					FVector2D(86.0f, 42.0f),
					8);

				UTextBlock* Duration = MakeScreenText(
					*FString::Printf(TEXT("BuildCityCardDuration_%d"), TypeId),
					17,
					FLinearColor(0.8509804f, 0.83137256f, 0.7294118f, 1),
					false,
					ETextJustify::Center);
				ApplyCardFont(Duration, 18);
				Duration->SetText(
					FormatBuildDuration(BuildingType.Level1BuildSeconds));
				PlaceOnCanvas(
					Card,
					Duration,
					FVector2D(117.0f, 430.0f),
					FVector2D(184.0f, 42.0f),
					8);

				URokUnityIndexedButton* CardButton = MakeActionButton(
					*FString::Printf(TEXT("BuildCityCardButton_%d"), TypeId),
					1320 + Index,
					nullptr,
					FIntRect(),
					FIntPoint::ZeroValue,
					FVector2D(318.0f, 501.0f));
				CardButton->SetClickMethod(EButtonClickMethod::PreciseClick);
				CardButton->SetToolTipText(FText::Format(
					NSLOCTEXT(
						"RokBuildCity",
						"BuildableTooltip",
						"建造 {0}"),
					Name->GetText()));
				PlaceOnCanvas(
					Card,
					CardButton,
					FVector2D(7.0f, 0.2f),
					FVector2D(318.0f, 501.0f),
					10);
			}
			else
			{
				UTextBlock* Locked = MakeScreenText(
					*FString::Printf(TEXT("BuildCityCardLocked_%d"), TypeId),
					18,
					FLinearColor(0.8509804f, 0.83137256f, 0.7294118f, 1),
					false,
					ETextJustify::Center);
				ApplyCardFont(Locked, 18);
				Locked->SetText(
					BuildState == 2
						? NSLOCTEXT(
							"RokBuildCity",
							"MaximumBuilt",
							"已达建造上限")
						: FText::Format(
							NSLOCTEXT(
								"RokBuildCity",
								"TownHallUnlock",
								"市政厅达到 {0} 级后解锁"),
							FText::AsNumber(FMath::Max(
								TownHallLevel + 1,
								UnlockTownHallLevel))));
				Locked->SetAutoWrapText(true);
				PlaceOnCanvas(
					Card,
					Locked,
					FVector2D(46.0f, 373.0f),
					FVector2D(240.0f, 64.0f),
					8);
			}

			BuildingScroll->AddChild(
				WrapFixedSize(
					Card,
					*FString::Printf(
						TEXT("BuildCityCardSize_%d"),
						TypeId),
					FVector2D(332.0f, 501.4f)));
		}
		bBuildCityInitialScrollPending = true;
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(
			TEXT("BuildCitySideDecoration"),
			BuildCityImageAtlasPath,
			BuildCitySideDecorationRect,
			BuildCityImageAtlasSize,
			FVector2D(158.0f, 609.8f)),
		CapturePoint(1286.6f, 255.6f),
		FVector2D(158.0f, 609.8f),
		5);

	static const FVector2D TabPositions[] = {
		FVector2D(1307.3f, 393.25f),
		FVector2D(1307.3f, 527.85f),
		FVector2D(1307.3f, 662.45f),
	};
	static const FIntRect NormalTabIcons[] = {
		SpriteRect(434, 961, 69, 62),
		SpriteRect(322, 816, 64, 52),
		SpriteRect(162, 491, 78, 50),
	};
	static const FIntRect SelectedTabIcons[] = {
		SpriteRect(322, 870, 75, 67),
		SpriteRect(162, 431, 67, 58),
		SpriteRect(162, 543, 84, 56),
	};
	for (int32 Category = 0; Category < 3; ++Category)
	{
		const bool bSelected = Category == SelectedBuildCityCategory;
		const FVector2D TabPosition = FVector2D(
			TabPositions[Category].X,
			TabPositions[Category].Y + 45.0f);
		URokUnityIndexedButton* CategoryButton = MakeActionButton(
				*FString::Printf(TEXT("BuildCityCategory_%d"), Category),
				1300 + Category,
				BuildCityImageAtlasPath,
				bSelected
					? BuildCitySelectedTabRect
					: BuildCityNormalTabRect,
				BuildCityImageAtlasSize,
				FVector2D(116.5f, 119.3f));
		// UI_Item_BuildCityBtn: transition=None, no BtnAnimation/Animator.
		// BuildCityMediator.ChangeBtnState switches iconOn/iconOff immediately.
		CategoryButton->SetPressAnimationEnabled(false);
		PlaceOnCanvas(
			ModeCanvas,
			CategoryButton,
			CapturePoint(TabPosition.X, TabPosition.Y),
			FVector2D(116.5f, 119.3f),
			7);
		const FIntRect IconRect = bSelected
			? SelectedTabIcons[Category]
			: NormalTabIcons[Category];
		const FVector2D IconSize(
			static_cast<float>(IconRect.Width()),
			static_cast<float>(IconRect.Height()));
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(
				*FString::Printf(TEXT("BuildCityCategoryIcon_%d"), Category),
				BuildCityImageAtlasPath,
				IconRect,
				BuildCityImageAtlasSize,
				IconSize),
			CapturePoint(
				TabPosition.X + (116.5f - IconSize.X) * 0.5f,
				TabPosition.Y + (119.3f - IconSize.Y) * 0.5f),
			IconSize,
			8);
		if (Category < 2 && CategoryBuildableCounts[Category] > 0)
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					*FString::Printf(TEXT("BuildCityCategoryRed_%d"), Category),
					CommonAtlasPath,
					RedPointRect,
					CommonAtlasSize,
					FVector2D(27.0f, 29.0f)),
				CapturePoint(
					TabPosition.X + 91.0f,
					TabPosition.Y + 3.0f),
				FVector2D(27.0f, 29.0f),
				9);
			UTextBlock* RedCount = MakeScreenText(
				*FString::Printf(TEXT("BuildCityCategoryRedCount_%d"), Category),
				13,
				FLinearColor::White,
				true,
				ETextJustify::Center);
			RedCount->SetText(FText::AsNumber(
				CategoryBuildableCounts[Category]));
			PlaceOnCanvas(
				ModeCanvas,
				RedCount,
				CapturePoint(
					TabPosition.X + 91.0f,
					TabPosition.Y + 6.0f),
				FVector2D(27.0f, 22.0f),
				10);
		}
	}

	const TArray<UWidget*> DrawerChildren = ModeCanvas->GetAllChildren();
	BuildCityAnimatedRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BuildCityAnimatedRoot"));
	BuildCityAnimatedRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ModeCanvas, BuildCityAnimatedRoot, FVector2D::ZeroVector, FVector2D(1440, 810), 2);
	for (UWidget* Child : DrawerChildren)
	{
		if (Child->GetFName() == TEXT("BuildCityMaskClose")) continue;
		const auto* OldSlot = CastChecked<UCanvasPanelSlot>(Child->Slot);
		const FVector2D Position = OldSlot->GetPosition();
		const FVector2D Size = OldSlot->GetSize();
		const int32 ZOrder = OldSlot->GetZOrder();
		Child->RemoveFromParent();
		PlaceOnCanvas(BuildCityAnimatedRoot, Child, Position, Size, ZOrder);
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok build city screen: category=%d types=%d authority=local_preview."),
		SelectedBuildCityCategory,
		VisibleBuildCityTypeIds.Num());
}

void URokUnityFeatureScreenWidget::BuildWorkerQueue()
{
	BuildSplitWindowFrame(
		CapturePoint(160.0f, 100.0f),
		FVector2D(1120.0f, 700.0f),
		NSLOCTEXT("RokWorkerQueue", "Title", "工人队列"),
		CapturePoint(1210.0f, 92.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("WorkerQueueContentShade"),
			FLinearColor(0.045f, 0.047f, 0.043f, 0.99f)),
		CapturePoint(208.0f, 162.0f),
		FVector2D(1024.0f, 612.0f),
		2);

	URokBuildingUpgradeSubsystem* UpgradeSubsystem = nullptr;
	if (UWorld* World = GetWorld())
	{
		UpgradeSubsystem =
			World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	}

	AActor* ActiveUpgradeActor = UpgradeSubsystem
		? UpgradeSubsystem->GetFirstActiveUpgradeActor()
		: nullptr;
	FRokBuildingState ActiveUpgradeState;
	float ActiveUpgradeProgress = 0.0f;
	int32 ActiveUpgradeRemainingSeconds = 0;
	bool bHasActiveUpgrade = false;
	if (UpgradeSubsystem && ActiveUpgradeActor)
	{
		ActiveUpgradeState = UpgradeSubsystem
			->GetBuildingStateForActor(ActiveUpgradeActor);
		bHasActiveUpgrade = ActiveUpgradeState.bUpgradeInProgress;
		if (bHasActiveUpgrade)
		{
			ActiveUpgradeProgress = FMath::Clamp(
				UpgradeSubsystem->GetUpgradeProgress(ActiveUpgradeActor),
				0.0f,
				1.0f);
			ActiveUpgradeRemainingSeconds = FMath::Max(
				0,
				FMath::CeilToInt(
					UpgradeSubsystem->GetRemainingUpgradeSeconds(
						ActiveUpgradeActor)));
		}
	}
	bWorkerQueueRenderedBusy = bHasActiveUpgrade;
	FText PrimaryQueueTitle = NSLOCTEXT(
		"RokWorkerQueue",
		"PrimaryTitle",
		"您的建造队列正处于空闲状态");
	if (bHasActiveUpgrade)
	{
		PrimaryQueueTitle = FText::Format(
			NSLOCTEXT(
				"RokWorkerQueue",
				"ActivePrimaryTitle",
				"等级 {1}  {0} 正在升级中…"),
			ActiveUpgradeState.DisplayName,
			FText::AsNumber(FMath::Max(1, ActiveUpgradeState.Level + 1)));
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("WorkerQueuePrimaryShade"),
			FLinearColor(0.12f, 0.145f, 0.12f, 1.0f)),
		CapturePoint(208.0f, 186.5f),
		FVector2D(1024.0f, 137.0f),
		3);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(
			TEXT("WorkerQueuePrimaryIllustration"),
			WorkerMatrixAtlasPath,
			bHasActiveUpgrade ? WorkerBusyRect : WorkerIdleRect,
			WorkerMatrixAtlasSize,
			FVector2D(179.0f, 137.0f)),
		CapturePoint(1029.0f, 186.5f),
		FVector2D(179.0f, 137.0f),
		6);
	WorkerQueuePrimaryTitleText = MakeScreenText(
		TEXT("WorkerQueuePrimaryTitle"),
		22,
		FLinearColor(0.93f, 0.84f, 0.51f, 1.0f),
		true);
	WorkerQueuePrimaryTitleText->SetText(PrimaryQueueTitle);
	PlaceOnCanvas(
		ModeCanvas,
		WorkerQueuePrimaryTitleText,
		CapturePoint(460.0f, 200.0f),
		FVector2D(542.0f, 36.0f),
		5);

	WorkerQueueRuntimeStateText = MakeScreenText(
		TEXT("WorkerQueueRuntimeState"),
		16,
		FLinearColor(0.88f, 0.89f, 0.84f, 1.0f));
	if (UpgradeSubsystem)
	{
		WorkerQueueRuntimeStateText->SetText(
			bHasActiveUpgrade
				? NSLOCTEXT(
					"RokWorkerQueue",
					"RuntimeBusy",
					"城主，建筑工人们正热火朝天地辛勤工作着。")
				: NSLOCTEXT(
					"RokWorkerQueue",
					"RuntimeIdle",
					"城主，您的建筑工人随时可以开工！"));
	}
	else
	{
		WorkerQueueRuntimeStateText->SetText(
			NSLOCTEXT(
				"RokWorkerQueue",
				"RuntimeUnavailable",
				"建造运行态未加载，未显示推测的队列状态"));
	}
	PlaceOnCanvas(
		ModeCanvas,
		WorkerQueueRuntimeStateText,
		CapturePoint(460.0f, 235.0f),
		FVector2D(542.0f, 42.0f),
		5);

	WorkerQueuePrimaryProgress = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(),
		TEXT("WorkerQueuePrimaryProgress"));
	WorkerQueuePrimaryProgress->SetPercent(ActiveUpgradeProgress);
	WorkerQueuePrimaryProgress->SetFillColorAndOpacity(
		bHasActiveUpgrade
			? FLinearColor(0.25f, 0.76f, 0.34f, 1.0f)
			: FLinearColor(0.25f, 0.28f, 0.26f, 1.0f));
	WorkerQueuePrimaryProgress->SetVisibility(
		ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(
		ModeCanvas,
		WorkerQueuePrimaryProgress,
		CapturePoint(460.0f, 291.0f),
		FVector2D(546.0f, 24.0f),
		6);
	WorkerQueuePrimaryProgressText = MakeScreenText(
		TEXT("WorkerQueuePrimaryProgressText"),
		15,
		FLinearColor::White,
		true,
		ETextJustify::Center);
	WorkerQueuePrimaryProgressText->SetText(
		bHasActiveUpgrade
			? FText::Format(
				NSLOCTEXT(
					"RokWorkerQueue",
					"PrimaryProgressFormat",
					"剩余 {0}  ·  已完成 {1}%"),
				FormatBuildDuration(ActiveUpgradeRemainingSeconds),
				FText::AsNumber(FMath::RoundToInt(
					ActiveUpgradeProgress * 100.0f)))
			: FText::GetEmpty());
	PlaceOnCanvas(
		ModeCanvas,
		WorkerQueuePrimaryProgressText,
		CapturePoint(460.0f, 288.0f),
		FVector2D(546.0f, 29.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("WorkerQueuePrimaryAction"),
			1400,
			CommonAtlasPath,
			MediumBlueButtonRect,
			CommonAtlasSize,
			FVector2D(202.0f, 81.0f),
			bHasActiveUpgrade
				? NSLOCTEXT("RokWorkerQueue", "SpeedUp", "加速")
				: NSLOCTEXT("RokWorkerQueue", "StartBuild", "建造"),
			20,
			FLinearColor(0.04f, 0.14f, 0.24f, 1.0f),
			ESlateBrushDrawType::Box,
			MiniBlueMargin),
		CapturePoint(238.0f, 211.0f),
		FVector2D(202.0f, 81.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(
			TEXT("WorkerQueueCardDivider"),
			CommonAtlasPath,
			GroupLineRect,
			CommonAtlasSize,
			FVector2D(964.0f, 2.0f)),
		CapturePoint(238.0f, 323.0f),
		FVector2D(964.0f, 2.0f),
		6);

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("WorkerQueueSecondaryShade"),
			FLinearColor(0.09f, 0.09f, 0.085f, 1.0f)),
		CapturePoint(208.0f, 323.5f),
		FVector2D(1024.0f, 137.0f),
		3);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(
			TEXT("WorkerQueueSecondaryIllustration"),
			WorkerMatrixAtlasPath,
			WorkerLockedRect,
			WorkerMatrixAtlasSize,
			FVector2D(179.0f, 137.0f)),
		CapturePoint(1029.0f, 323.5f),
		FVector2D(179.0f, 137.0f),
		6);
	UTextBlock* SecondaryTitle = MakeScreenText(
		TEXT("WorkerQueueSecondaryTitle"),
		24,
		FLinearColor(0.65f, 0.64f, 0.59f, 1.0f),
		true);
	SecondaryTitle->SetText(
		NSLOCTEXT(
			"RokWorkerQueue",
			"SecondaryTitle",
			"第二建造队列  ·  账号状态未同步"));
	PlaceOnCanvas(
		ModeCanvas,
		SecondaryTitle,
		CapturePoint(460.0f, 337.0f),
		FVector2D(542.0f, 36.0f),
		5);
	UTextBlock* SecondaryNote = MakeScreenText(
		TEXT("WorkerQueueSecondaryNote"),
		17,
		FLinearColor(0.70f, 0.69f, 0.64f, 1.0f));
	SecondaryNote->SetText(
		NSLOCTEXT(
			"RokWorkerQueue",
			"SecondaryNote",
			"队列状态由账号确认；此本地预览未同步是否已开通。"));
	PlaceOnCanvas(
		ModeCanvas,
		SecondaryNote,
		CapturePoint(460.0f, 376.0f),
		FVector2D(542.0f, 58.0f),
		5);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("WorkerQueueSecondQueueUnlockOrExtend"),
			1420,
			CommonAtlasPath,
			MediumGoldButtonRect,
			CommonAtlasSize,
			FVector2D(202.0f, 81.0f),
			NSLOCTEXT(
				"RokWorkerQueue",
				"UnlockOrExtend",
				"账号状态说明"),
			19,
			FLinearColor(0.25f, 0.13f, 0.02f, 1.0f),
			ESlateBrushDrawType::Box,
			FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		CapturePoint(238.0f, 348.0f),
		FVector2D(202.0f, 81.0f),
		7);
	UTextBlock* ContractBoundary = MakeScreenText(
		TEXT("WorkerQueueContractBoundary"),
		18,
		FLinearColor(0.78f, 0.76f, 0.68f, 1.0f),
		false,
		ETextJustify::Center);
	ContractBoundary->SetText(NSLOCTEXT(
		"RokWorkerQueue",
		"ContractBoundary",
		"第二队列由账号服务器管理；本地不会提前解锁、扣费或延长有效期。"));
	PlaceOnCanvas(
		ModeCanvas,
		ContractBoundary,
		CapturePoint(280.0f, 500.0f),
		FVector2D(880.0f, 34.0f),
		6);
}

void URokUnityFeatureScreenWidget::BuildHospital()
{
	BuildSplitWindowFrame(
		CapturePoint(146.0f, 73.0f),
		FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokHospital", "Title", "医院"),
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("HospitalContentShade"),
			FLinearColor(0.035f, 0.04f, 0.038f, 0.99f)),
		CapturePoint(166.0f, 151.0f),
		FVector2D(1108.0f, 620.0f),
		2);

	URokBuildingUpgradeSubsystem* UpgradeSubsystem = nullptr;
	if (UWorld* World = GetWorld())
	{
		UpgradeSubsystem =
			World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	}
	int32 BuildingLevel = 1;
	FText BuildingName = NSLOCTEXT(
		"RokHospital",
		"PreviewBuildingName",
		"医院预览");
	if (UpgradeSubsystem && FeatureContextActor.IsValid())
	{
		const FRokBuildingState State = UpgradeSubsystem
			->GetBuildingStateForActor(FeatureContextActor.Get());
		BuildingLevel = FMath::Max(1, State.Level);
		if (!State.DisplayName.IsEmpty())
		{
			BuildingName = State.DisplayName;
		}
	}

	URokUnityGameplayCatalogSubsystem* GameplayCatalog =
		GetGameplayCatalogSubsystem();
	if (GameplayCatalog && !GameplayCatalog->IsCatalogLoaded())
	{
		GameplayCatalog->LoadCatalog();
	}
	FRokUnityCatalogBuildingType HospitalType;
	const bool bHasHospitalCatalog = GameplayCatalog
		&& GameplayCatalog->GetBuildingType(14, HospitalType);
	const int32 HospitalCapacity = bHasHospitalCatalog
		? HospitalType.GetHospitalCapacity(BuildingLevel)
		: 0;

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("HospitalFacilityPanel"),
			FLinearColor(0.10f, 0.125f, 0.11f, 1.0f)),
		CapturePoint(190.0f, 180.0f),
		FVector2D(310.0f, 552.0f),
		3);
	UTextBlock* BuildingTitle = MakeScreenText(
		TEXT("HospitalBuildingTitle"),
		25,
		FLinearColor(0.93f, 0.84f, 0.52f, 1.0f),
		true,
		ETextJustify::Center);
	BuildingTitle->SetText(FText::Format(
		NSLOCTEXT("RokHospital", "BuildingLevelFormat", "{0}  Lv.{1}"),
		BuildingName,
		FText::AsNumber(BuildingLevel)));
	PlaceOnCanvas(
		ModeCanvas,
		BuildingTitle,
		CapturePoint(210.0f, 205.0f),
		FVector2D(270.0f, 38.0f),
		5);

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("HospitalCapacityBadge"),
			FLinearColor(0.055f, 0.065f, 0.06f, 1.0f)),
		CapturePoint(220.0f, 270.0f),
		FVector2D(250.0f, 94.0f),
		4);
	UTextBlock* CapacityValue = MakeScreenText(
		TEXT("HospitalCapacityValue"),
		23,
		FLinearColor(0.85f, 0.91f, 0.84f, 1.0f),
		true,
		ETextJustify::Center);
	CapacityValue->SetText(HospitalCapacity > 0
		? FText::Format(
			NSLOCTEXT(
				"RokHospital",
				"CapacityKnown",
				"伤兵  -- / {0}"),
			FText::AsNumber(HospitalCapacity))
		: NSLOCTEXT(
			"RokHospital",
			"CapacityUnavailable",
			"伤兵  -- / --"));
	PlaceOnCanvas(
		ModeCanvas,
		CapacityValue,
		CapturePoint(230.0f, 292.0f),
		FVector2D(230.0f, 40.0f),
		6);
	UTextBlock* CapacitySource = MakeScreenText(
		TEXT("HospitalCapacitySource"),
		14,
		FLinearColor(0.58f, 0.66f, 0.60f, 1.0f),
		false,
		ETextJustify::Center);
	CapacitySource->SetText(HospitalCapacity > 0
		? NSLOCTEXT(
			"RokHospital",
			"CapacityConfigSource",
			"容量来自 Unity BuildingHospital 配置")
		: NSLOCTEXT(
			"RokHospital",
			"CapacityConfigMissing",
			"医院容量配置未加载"));
	PlaceOnCanvas(
		ModeCanvas,
		CapacitySource,
		CapturePoint(220.0f, 344.0f),
		FVector2D(250.0f, 42.0f),
		6);

	UTextBlock* WoundedState = MakeScreenText(
		TEXT("HospitalWoundedState"),
		18,
		FLinearColor(0.78f, 0.80f, 0.76f, 1.0f),
		false,
		ETextJustify::Center);
	WoundedState->SetAutoWrapText(true);
	WoundedState->SetText(NSLOCTEXT(
		"RokHospital",
		"WoundedStateBoundary",
		"伤兵数量、治疗队列与完成时间由服务器下发。\n当前本地预览没有伪造伤兵。"));
	PlaceOnCanvas(
		ModeCanvas,
		WoundedState,
		CapturePoint(220.0f, 420.0f),
		FVector2D(250.0f, 105.0f),
		5);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("HospitalCapacityInfo"),
			2000,
			CommonAtlasPath,
			MediumBlueButtonRect,
			CommonAtlasSize,
			FVector2D(190.0f, 54.0f),
			NSLOCTEXT("RokHospital", "CapacityInfo", "容量说明"),
			18,
			FLinearColor(0.04f, 0.14f, 0.24f, 1.0f),
			ESlateBrushDrawType::Box,
			MiniBlueMargin),
		CapturePoint(250.0f, 642.0f),
		FVector2D(190.0f, 54.0f),
		7);

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("HospitalTreatmentPanel"),
			FLinearColor(0.075f, 0.08f, 0.075f, 1.0f)),
		CapturePoint(520.0f, 180.0f),
		FVector2D(724.0f, 552.0f),
		3);
	UTextBlock* TreatmentTitle = MakeScreenText(
		TEXT("HospitalTreatmentTitle"),
		25,
		FLinearColor(0.90f, 0.84f, 0.60f, 1.0f),
		true);
	TreatmentTitle->SetText(NSLOCTEXT(
		"RokHospital",
		"TreatmentTitle",
		"伤兵与治疗"));
	PlaceOnCanvas(
		ModeCanvas,
		TreatmentTitle,
		CapturePoint(548.0f, 205.0f),
		FVector2D(300.0f, 38.0f),
		5);

	static const FText TroopLabels[] = {
		NSLOCTEXT("RokHospital", "InfantryWounded", "步兵伤兵"),
		NSLOCTEXT("RokHospital", "CavalryWounded", "骑兵伤兵"),
		NSLOCTEXT("RokHospital", "ArcherWounded", "弓兵伤兵"),
		NSLOCTEXT("RokHospital", "SiegeWounded", "攻城伤兵")};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector2D RowPosition = CapturePoint(
			548.0f,
			252.0f + Index * 76.0f);
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				*FString::Printf(TEXT("HospitalTroopRow_%d"), Index),
				FLinearColor(0.045f, 0.052f, 0.047f, 1.0f)),
			RowPosition,
			FVector2D(664.0f, 66.0f),
			4);
		FIntRect TroopRect;
		if (ResolveTroopHeadRect(Index + 1, 1, TroopRect))
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					*FString::Printf(TEXT("HospitalTroopIcon_%d"), Index),
					TroopHeadAtlasPath,
					TroopRect,
					TroopHeadAtlasSize,
					FVector2D(56.0f)),
				RowPosition + FVector2D(8.0f, 5.0f),
				FVector2D(56.0f),
				5);
		}
		UTextBlock* TroopLabel = MakeScreenText(
			*FString::Printf(TEXT("HospitalTroopLabel_%d"), Index),
			19,
			FLinearColor(0.85f, 0.86f, 0.82f, 1.0f));
		TroopLabel->SetText(TroopLabels[Index]);
		PlaceOnCanvas(
			ModeCanvas,
			TroopLabel,
			RowPosition + FVector2D(82.0f, 17.0f),
			FVector2D(270.0f, 32.0f),
			5);
		UTextBlock* TroopValue = MakeScreenText(
			*FString::Printf(TEXT("HospitalTroopValue_%d"), Index),
			20,
			FLinearColor(0.74f, 0.76f, 0.72f, 1.0f),
			true,
			ETextJustify::Right);
		TroopValue->SetText(FText::FromString(TEXT("--")));
		PlaceOnCanvas(
			ModeCanvas,
			TroopValue,
			RowPosition + FVector2D(510.0f, 16.0f),
			FVector2D(120.0f, 34.0f),
			5);
	}

	UTextBlock* CostState = MakeScreenText(
		TEXT("HospitalTreatmentCost"),
		17,
		FLinearColor(0.72f, 0.73f, 0.69f, 1.0f));
	CostState->SetText(NSLOCTEXT(
		"RokHospital",
		"TreatmentCostBoundary",
		"治疗消耗：粮食 --  木材 --  石料 --  金币 --     治疗时间：--"));
	PlaceOnCanvas(
		ModeCanvas,
		CostState,
		CapturePoint(548.0f, 570.0f),
		FVector2D(664.0f, 34.0f),
		5);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("HospitalMaxButton"),
			2003,
			CommonAtlasPath,
			MediumBlueButtonRect,
			CommonAtlasSize,
			FVector2D(120.0f, 58.0f),
			NSLOCTEXT("RokHospital", "Maximum", "最大"),
			18,
			FLinearColor(0.04f, 0.14f, 0.24f, 1.0f),
			ESlateBrushDrawType::Box,
			MiniBlueMargin),
		CapturePoint(548.0f, 642.0f),
		FVector2D(120.0f, 58.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("HospitalHealButton"),
			2001,
			CommonAtlasPath,
			MediumBlueButtonRect,
			CommonAtlasSize,
			FVector2D(245.0f, 58.0f),
			NSLOCTEXT("RokHospital", "HealBoundary", "治疗（待服务器）"),
			18,
			FLinearColor(0.04f, 0.14f, 0.24f, 1.0f),
			ESlateBrushDrawType::Box,
			MiniBlueMargin),
		CapturePoint(690.0f, 642.0f),
		FVector2D(245.0f, 58.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("HospitalHealNowButton"),
			2002,
			CommonAtlasPath,
			MediumGoldButtonRect,
			CommonAtlasSize,
			FVector2D(255.0f, 58.0f),
			NSLOCTEXT("RokHospital", "HealNowBoundary", "立即治疗（待服务器）"),
			18,
			FLinearColor(0.25f, 0.13f, 0.02f, 1.0f),
			ESlateBrushDrawType::Box,
			FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		CapturePoint(957.0f, 642.0f),
		FVector2D(255.0f, 58.0f),
		7);
	HospitalExplanationWidget=WidgetTree->ConstructWidget<URokHospitalExplanationWidget>(URokHospitalExplanationWidget::StaticClass(),TEXT("HospitalExplanationWidget"));
	if(HospitalExplanationWidget->InitializeExplanation())
	{
		PlaceOnCanvas(ModeCanvas,HospitalExplanationWidget,HospitalExplanationWidget->GetSourcePosition(),HospitalExplanationWidget->GetSourceSize(),9);
		HospitalExplanationWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		UE_LOG(LogTemp,Error,TEXT("Hospital explanation: %s"),*HospitalExplanationWidget->GetLoadError());
		HospitalExplanationWidget=nullptr;
	}
	auto* Back=MakeActionButton(TEXT("HospitalExplanationBackButton"),2004,CommonAtlasPath,CommanderBackRect,CommonAtlasSize,FVector2D(69));
	PlaceOnCanvas(ModeCanvas,Back,CapturePoint(148.0f,86.0f),FVector2D(69),10);Back->SetVisibility(ESlateVisibility::Collapsed);
}

void URokUnityFeatureScreenWidget::SetHospitalExplanationVisible(bool bVisible)
{
	if(ActiveMode!=ERokUnityFeatureScreenMode::Hospital||!HospitalExplanationWidget)return;
	if(bVisible&&!bHospitalExplanationVisible)
	{
		HospitalHiddenControls.Reset();
		for(UWidget* Child:ModeCanvas->GetAllChildren())
		{
			const FString Name=Child->GetName();
			if(Name.StartsWith(TEXT("HospitalCapacity"))||Name.StartsWith(TEXT("HospitalTreatment"))||Name.StartsWith(TEXT("HospitalTroop"))
				||Name==TEXT("HospitalWoundedState")||Name==TEXT("HospitalMaxButton")||Name==TEXT("HospitalHealButton")||Name==TEXT("HospitalHealNowButton"))
			{HospitalHiddenControls.Add(Child,Child->GetVisibility());Child->SetVisibility(ESlateVisibility::Collapsed);}
		}
	}
	else if(!bVisible)
	{for(const auto& Pair:HospitalHiddenControls)if(Pair.Key.IsValid())Pair.Key->SetVisibility(Pair.Value);HospitalHiddenControls.Reset();}
	bHospitalExplanationVisible=bVisible;
	HospitalExplanationWidget->SetVisibility(bVisible?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
	if(auto* Back=GetWidgetFromName(TEXT("HospitalExplanationBackButton")))Back->SetVisibility(bVisible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
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

	if (SelectedCityManagerPage == 0)
	{
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
		static const TCHAR* LevelLabels[] = {
			TEXT("10-12"), TEXT("13-14"), TEXT("15"), TEXT("16"), TEXT("17"),
			TEXT("18"), TEXT("19"), TEXT("20"), TEXT("21"), TEXT("22"),
			TEXT("23"), TEXT("24"), TEXT("25")};
		static const TCHAR* DurationLabels[] = {
			TEXT("60秒"), TEXT("2分钟"), TEXT("3分钟"), TEXT("4分钟"), TEXT("5分钟"),
			TEXT("6分钟"), TEXT("7分钟"), TEXT("8分钟"), TEXT("9分钟"), TEXT("10分钟"),
			TEXT("11分钟"), TEXT("12分钟"), TEXT("15分钟")};
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
	UTextBlock* DetailTitle = MakeScreenText(
		TEXT("CityManagerDetailTitle"), 28,
		FLinearColor(0.96f, 0.88f, 0.58f, 1.0f), true,
		ETextJustify::Center);
	DetailTitle->SetText(FText::FromString(SelectedGroup->Name));
	PlaceOnCanvas(ModeCanvas, DetailTitle, FVector2D(208.0f, 132.0f), FVector2D(1024.0f, 44.0f), 5);
	UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), TEXT("CityManagerDetailScroll"));
	DetailScroll->SetOrientation(Orient_Vertical);
	DetailScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(ModeCanvas, DetailScroll, FVector2D(208.0f, 182.0f), FVector2D(1024.0f, 547.0f), 4);
	UCanvasPanel* DetailContent = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CityManagerDetailContent"));

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
	DetailScroll->AddChild(WrapFixedSize(DetailContent, TEXT("CityManagerDetailContentSize"), FVector2D(1024.0f, DetailHeight)));
	for (int32 Index = 0; Index < DetailItems.Num(); ++Index)
	{
		const FRokUnityEconomyItem& Item = DetailItems[Index];
		const float Y = Index * 156.0f;
		PlaceOnCanvas(DetailContent, MakeAtlasBorder(*FString::Printf(TEXT("CityManagerDetailRowBackground_%d"), Index), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(1024.0f, 154.0f), CommonPanelMargin), FVector2D(0.0f, Y), FVector2D(1024.0f, 154.0f), 0);
		const TCHAR* IconAtlasPath = nullptr;
		FIntRect IconRect;
		FIntPoint IconAtlasSize;
		if (ResolveEconomyItemIcon(Item, IconAtlasPath, IconRect, IconAtlasSize))
		{
			PlaceOnCanvas(DetailContent, MakeAtlasImage(*FString::Printf(TEXT("CityManagerDetailIcon_%d"), Index), IconAtlasPath, IconRect, IconAtlasSize, FVector2D(112.0f)), FVector2D(884.0f, Y + 21.0f), FVector2D(112.0f), 3);
		}
		UTextBlock* ItemName = MakeScreenText(*FString::Printf(TEXT("CityManagerDetailItemName_%d"), Index), 22, FLinearColor(0.96f, 0.88f, 0.58f, 1.0f), true, ETextJustify::Right);
		ItemName->SetText(FText::FromString(Item.DisplayNameZhCn));
		PlaceOnCanvas(DetailContent, ItemName, FVector2D(410.0f, Y + 20.0f), FVector2D(450.0f, 35.0f), 3);
		UTextBlock* ItemDescription = MakeScreenText(*FString::Printf(TEXT("CityManagerDetailItemDescription_%d"), Index), 17, FLinearColor(0.82f, 0.82f, 0.78f, 1.0f), false, ETextJustify::Right);
		ItemDescription->SetAutoWrapText(true);
		ItemDescription->SetText(FText::FromString(ResolveEconomyItemDescription(Item)));
		PlaceOnCanvas(DetailContent, ItemDescription, FVector2D(330.0f, Y + 60.0f), FVector2D(530.0f, 65.0f), 3);
		PlaceOnCanvas(DetailContent, MakeActionButton(*FString::Printf(TEXT("CityManagerDetailAction_%d"), Index), 2160 + Index, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(188.0f, 54.0f), NSLOCTEXT("RokCityManager", "InventoryUnavailable", "库存未同步"), 16, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), FVector2D(50.0f, Y + 51.0f), FVector2D(188.0f, 54.0f), 7);
	}
	UE_LOG(LogTemp, Display, TEXT("Rok city manager: page=buff_detail group=%d config_items=%d authority=inventory_unsynced."), SelectedGroup->GroupId, DetailItems.Num());
}

void URokUnityFeatureScreenWidget::BuildArmyShow()
{
	BuildSplitWindowFrame(
		CapturePoint(133.5f, 85.5f),
		FVector2D(1173.0f, 729.0f),
		NSLOCTEXT("RokArmyShow", "Title", "部队一览"),
		CapturePoint(1230.0f, 77.0f));

	if (bArmyShowRulesVisible)
	{
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

	UScrollBox* ArmyList = WidgetTree->ConstructWidget<UScrollBox>(
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

	// pl_blet and sv_armys share ContentShow; keep the frame/back button still.
	for (int32 Index = ArmyListFirstChild; Index < ModeCanvas->GetChildrenCount(); ++Index)
	{
		ArmyShowPanelAnimationWidgets.Add(ModeCanvas->GetChildAt(Index));
	}
	AdvanceArmyShowPanelAnimation(0.0f);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok ArmyShow screen: source=UI_Win_ArmyShow rendered_rows=0 placeholder_rows=0 authority=server."));
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

void URokUnityFeatureScreenWidget::BuildSettingsHub()
{
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
	DateText->SetText(FText::Format(
		NSLOCTEXT("RokSettings", "DateFormat", "日期  {0}"),
		FText::FromString(FDateTime::Now().ToString(TEXT("%Y-%m-%d")))));
	PlaceOnCanvas(
		ModeCanvas,
		DateText,
		CapturePoint(266.0f, 145.0f),
		FVector2D(396.0f, 40.0f),
		5);

	FString ProjectVersion = FString::Printf(
		TEXT("%s Preview"),
		FApp::GetProjectName());
	if (ProjectVersion.IsEmpty())
	{
		ProjectVersion = TEXT("UE 5.8");
	}
	UTextBlock* VersionText = MakeScreenText(
		TEXT("SettingsHubVersion"),
		17,
		FLinearColor(0.78f, 0.78f, 0.75f, 1.0f),
		false,
		ETextJustify::Center);
	VersionText->SetText(FText::Format(
		NSLOCTEXT("RokSettings", "VersionFormat", "版本  {0}"),
		FText::FromString(ProjectVersion)));
	PlaceOnCanvas(
		ModeCanvas,
		VersionText,
		CapturePoint(778.0f, 145.0f),
		FVector2D(396.0f, 40.0f),
		5);

	static const FName ButtonNames[] = {
		TEXT("SettingsHubCharacterButton"),
		TEXT("SettingsHubNotificationButton"),
		TEXT("SettingsHubGeneralButton"),
		TEXT("SettingsHubLanguageButton"),
		TEXT("SettingsHubCommunityButton"),
		TEXT("SettingsHubHelpButton"),
		TEXT("SettingsHubAccountButton")};
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
	static const int32 ActionIds[] = {1455, 1454, 1453, 1450, 1458, 1457, 1456};
	static const bool Enabled[] = {false, true, true, true, false, true, true};
	static const FText GateLabels[] = {
		NSLOCTEXT("RokSettings", "CharacterGate", "需服务器"),
		FText::GetEmpty(),
		FText::GetEmpty(),
		FText::GetEmpty(),
		NSLOCTEXT("RokSettings", "CommunityGate", "待平台接入"),
		NSLOCTEXT("RokSettings", "HelpGate", "待客服接入"),
		FText::GetEmpty()};

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ButtonNames); ++Index)
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
			ButtonNames[Index],
			ActionIds[Index],
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
	FTextBlockStyle Style; Style.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15)).SetColorAndOpacity(FSlateColor(FLinearColor::White)).SetShadowOffset(FVector2D::ZeroVector);
	auto* Styles = NewObject<UDataTable>(Recommendation); Styles->RowStruct = FRichTextStyleRow::StaticStruct();
	FRichTextStyleRow Default; Default.TextStyle = Style; Styles->AddRow(TEXT("Default"), Default);
	FRichTextStyleRow Green; Green.TextStyle = Style; Green.TextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(83.f / 255, 219.f / 255, 91.f / 255, 1)));
	Styles->AddRow(TEXT("Recommended"), Green); Recommendation->SetTextStyleSet(Styles); Recommendation->SetDefaultTextStyle(Style);
	Recommendation->SetText(FText::FromString(FString::Printf(TEXT("根据您的设备性能，我们推荐您选择图像质量:<Recommended>%s</>"),
		Levels[RecommendedTier(FPlatformMemory::GetConstants().TotalPhysical / (1024ull * 1024ull)) - 1])));
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
	PlaySourceScrollFeedback();
}

void URokUnityFeatureScreenWidget::BuildGeneralSettings()
{
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
	TArray<FRokPersonalityToggleDefinition> Definitions;
	if (Settings)
	{
		Definitions = Settings->GetPersonalityToggleDefinitions();
		Definitions.Sort(
			[](const FRokPersonalityToggleDefinition& Left,
				const FRokPersonalityToggleDefinition& Right)
			{
				return Left.UnityDisplayOrder < Right.UnityDisplayOrder;
			});
	}
	for (const FRokPersonalityToggleDefinition& Definition : Definitions)
	{
		if (Definition.bVisibleInUnity)
		{
			VisiblePersonalityToggleNames.Add(Definition.Name);
		}
	}

	for (int32 Index = 0; Index < VisiblePersonalityToggleNames.Num(); ++Index)
	{
		const FName ToggleName = VisiblePersonalityToggleNames[Index];
		const FRokPersonalityToggleDefinition* Definition = Definitions.FindByPredicate(
			[ToggleName](const FRokPersonalityToggleDefinition& Candidate)
			{
				return Candidate.Name == ToggleName;
			});
		if (!Definition)
		{
			continue;
		}
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
		GateText->SetText(
			Definition->UnityResetSeconds > 0
				? NSLOCTEXT("RokGeneralSettings", "ServerTimedGate", "由服务器时间控制，状态尚未同步")
				: NSLOCTEXT("RokGeneralSettings", "ConsumerGate", "相关功能尚未接入，当前不可更改"));
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
	static const FName Names[] = { TEXT("LanguageOptionEnglishButton"), TEXT("LanguageOptionArabicButton"), TEXT("LanguageOptionChineseButton"), TEXT("LanguageOptionTurkishButton") };
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
		PlaceOnCanvas(Cell,MakeActionButton(Names[Index],1460+Index,nullptr,FIntRect(),FIntPoint::ZeroValue,FVector2D(388,114)),FVector2D::ZeroVector,FVector2D(388,114),4);
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

	static const FText PageLabels[] = {
		NSLOCTEXT("RokArmyStatistics", "AllTroops", "本地汇总"),
		NSLOCTEXT("RokArmyStatistics", "InsideCity", "城内"),
		NSLOCTEXT("RokArmyStatistics", "OutsideCity", "城外")};
	for (int32 Page = 0; Page < 3; ++Page)
	{
		const bool bSelected =
			Page == SelectedArmyStatisticsPage;
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(
					TEXT("ArmyStatisticsPage_%d"),
					Page),
				1600 + Page,
				CommonAtlasPath,
				bSelected
					? PageSelectedRect
					: PageNormalRect,
				CommonAtlasSize,
				FVector2D(330.0f, 48.0f),
				PageLabels[Page],
				23,
				bSelected
					? FLinearColor::White
					: FLinearColor(0.72f, 0.72f, 0.70f, 1.0f),
				ESlateBrushDrawType::Box,
				FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
			CapturePoint(220.0f + Page * 330.0f, 166.0f),
			FVector2D(330.0f, 48.0f),
			6);
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

	if (SelectedArmyStatisticsPage == 0)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				TEXT("ArmyStatisticsSummaryShade"),
				FLinearColor(0.18f, 0.19f, 0.18f, 0.96f)),
			CapturePoint(220.0f, 232.0f),
			FVector2D(990.0f, 112.0f),
			4);
		UTextBlock* TotalCount = MakeScreenText(
			TEXT("ArmyStatisticsTotalCount"),
			25,
			FLinearColor(0.96f, 0.90f, 0.65f, 1.0f),
			true);
		TotalCount->SetText(FText::Format(
			NSLOCTEXT(
				"RokArmyStatistics",
				"TotalCountFormat",
				"本地预览兵力：{0}"),
			FText::AsNumber(Summary.TotalTroopCount)));
		PlaceOnCanvas(
			ModeCanvas,
			TotalCount,
			CapturePoint(250.0f, 252.0f),
			FVector2D(330.0f, 38.0f),
			6);
		UTextBlock* TotalPower = MakeScreenText(
			TEXT("ArmyStatisticsTotalPower"),
			25,
			FLinearColor(0.96f, 0.90f, 0.65f, 1.0f),
			true);
		TotalPower->SetText(FText::Format(
			NSLOCTEXT(
				"RokArmyStatistics",
				"TotalPowerFormat",
				"本地预览战力：{0}"),
			FText::AsNumber(Summary.TotalTroopPower)));
		PlaceOnCanvas(
			ModeCanvas,
			TotalPower,
			CapturePoint(600.0f, 252.0f),
			FVector2D(330.0f, 38.0f),
			6);
	}

	UTextBlock* DataSource = MakeScreenText(
		TEXT("ArmyStatisticsDataSource"),
		17,
		FLinearColor(0.76f, 0.76f, 0.73f, 1.0f));
	DataSource->SetText(
		!bRuntimeLoaded
			? NSLOCTEXT(
				"RokArmyStatistics",
				"RuntimeUnavailable",
				"运行态尚未加载，当前显示安全空态。")
			: SelectedArmyStatisticsPage == 0
				? NSLOCTEXT(
					"RokArmyStatistics",
					"BootstrapSource",
					"本地预览汇总：仅包含本地引导兵力；非账号部队状态。")
				: NSLOCTEXT(
					"RokArmyStatistics",
					"LocationUnavailableSource",
					"位置归属数据未接入；不按配置兵种推断城内或城外。"));
	PlaceOnCanvas(
		ModeCanvas,
		DataSource,
		CapturePoint(250.0f, 301.0f),
		FVector2D(850.0f, 28.0f),
		6);

	if (SelectedArmyStatisticsPage != 0)
	{
		const bool bInsideCity = SelectedArmyStatisticsPage == 1;
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				bInsideCity
					? TEXT("ArmyStatisticsInsideEmptyShade")
					: TEXT("ArmyStatisticsOutsideEmptyShade"),
				FLinearColor(0.13f, 0.14f, 0.13f, 0.96f)),
			CapturePoint(220.0f, 364.0f),
			FVector2D(990.0f, 370.0f),
			4);
		UTextBlock* Empty = MakeScreenText(
			bInsideCity
				? TEXT("ArmyStatisticsInsideEmpty")
				: TEXT("ArmyStatisticsOutsideEmpty"),
			23,
			FLinearColor(0.84f, 0.84f, 0.80f, 1.0f),
			false,
			ETextJustify::Center);
		Empty->SetText(
			bInsideCity
			? NSLOCTEXT(
				"RokArmyStatistics",
				"InsideLocationUnavailable",
				"城内兵力位置数据尚未接入。\n当前无法按城内归属显示部队。")
			: NSLOCTEXT(
				"RokArmyStatistics",
				"OutsideLocationUnavailable",
				"城外兵力位置数据尚未接入。\n当前无法按城外归属显示部队。"));
		Empty->SetAutoWrapText(true);
		PlaceOnCanvas(
			ModeCanvas,
			Empty,
			CapturePoint(345.0f, 490.0f),
			FVector2D(740.0f, 120.0f),
			6);
		return;
	}

	UScrollBox* TroopList =
		WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(),
			TEXT("ArmyStatisticsTroopList"));
	TroopList->SetOrientation(Orient_Vertical);
	TroopList->SetScrollBarVisibility(
		ESlateVisibility::Visible);
	PlaceOnCanvas(
		ModeCanvas,
		TroopList,
		CapturePoint(220.0f, 364.0f),
		FVector2D(990.0f, 370.0f),
		5);

	for (int32 Index = 0; Index < TroopStacks.Num(); ++Index)
	{
		const FRokUnityRuntimeTroopStack& Stack =
			TroopStacks[Index];
		UCanvasPanel* Row =
			WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(),
				*FString::Printf(
					TEXT("ArmyStatisticsRow_%d"),
					Index));
		TroopList->AddChild(
			WrapFixedSize(
				Row,
				*FString::Printf(
					TEXT("ArmyStatisticsRowSize_%d"),
					Index),
				FVector2D(965.0f, 92.0f)));
		PlaceOnCanvas(
			Row,
			MakeSolidBorder(
				*FString::Printf(
					TEXT("ArmyStatisticsRowShade_%d"),
					Index),
				Index % 2 == 0
					? FLinearColor(0.20f, 0.21f, 0.20f, 0.98f)
					: FLinearColor(0.16f, 0.17f, 0.16f, 0.98f)),
			FVector2D::ZeroVector,
			FVector2D(955.0f, 84.0f),
			0);
		UTextBlock* Name = MakeScreenText(
			*FString::Printf(
				TEXT("ArmyStatisticsRowName_%d"),
				Index),
			23,
			FLinearColor(0.96f, 0.90f, 0.65f, 1.0f),
			true);
		Name->SetText(FText::Format(
			NSLOCTEXT(
				"RokArmyStatistics",
				"TroopNameFormat",
				"{0} T{1}  ·  配置 {2}"),
			ResolveTroopTypeLabel(Stack.ArmsType),
			FText::AsNumber(Stack.Level),
			FText::AsNumber(Stack.TroopId)));
		PlaceOnCanvas(
			Row,
			Name,
			FVector2D(24.0f, 12.0f),
			FVector2D(450.0f, 36.0f),
			2);
		UTextBlock* Count = MakeScreenText(
			*FString::Printf(
				TEXT("ArmyStatisticsRowCount_%d"),
				Index),
			21,
			FLinearColor::White,
			true);
		Count->SetText(FText::Format(
			NSLOCTEXT(
				"RokArmyStatistics",
				"TroopCountFormat",
				"数量 {0}"),
			FText::AsNumber(Stack.Count)));
		PlaceOnCanvas(
			Row,
			Count,
			FVector2D(520.0f, 13.0f),
			FVector2D(180.0f, 34.0f),
			2);
		UTextBlock* Power = MakeScreenText(
			*FString::Printf(
				TEXT("ArmyStatisticsRowPower_%d"),
				Index),
			21,
			FLinearColor::White,
			true);
		Power->SetText(FText::Format(
			NSLOCTEXT(
				"RokArmyStatistics",
				"TroopPowerFormat",
				"战力 {0}"),
			FText::AsNumber(Stack.TotalPower)));
		PlaceOnCanvas(
			Row,
			Power,
			FVector2D(735.0f, 13.0f),
			FVector2D(200.0f, 34.0f),
			2);
		UTextBlock* Source = MakeScreenText(
			*FString::Printf(
				TEXT("ArmyStatisticsRowSource_%d"),
				Index),
			16,
			FLinearColor(0.72f, 0.72f, 0.70f, 1.0f));
		Source->SetText(FText::Format(
			NSLOCTEXT(
				"RokArmyStatistics",
				"TroopSourceFormat",
				"单兵战力 {0}  ·  训练耗时 {1} 秒"),
			FText::AsNumber(Stack.PowerPerUnit),
			FText::AsNumber(Stack.TrainSeconds)));
		PlaceOnCanvas(
			Row,
			Source,
			FVector2D(24.0f, 50.0f),
			FVector2D(650.0f, 28.0f),
			2);
	}
}

void URokUnityFeatureScreenWidget::BuildStore()
{
	BuildSplitWindowFrame(
		CapturePoint(170.0f, 73.0f),
		FVector2D(1100.0f, 754.0f),
		NSLOCTEXT("RokStore", "Title", "商店"),
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("StoreContentShade"),
			FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(184.0f, 153.0f),
		FVector2D(1072.0f, 664.0f),
		2);

	static const int32 StoreCategoryTypes[] = {1, 2, 3, 5};
	static const FText StoreCategoryLabels[] = {
		NSLOCTEXT("RokStore", "Resources", "资源"),
		NSLOCTEXT("RokStore", "Speedups", "加速"),
		NSLOCTEXT("RokStore", "Boosts", "增益"),
		NSLOCTEXT("RokStore", "Other", "其他")};
	for (int32 Category = 0; Category < 4; ++Category)
	{
		const bool bSelected =
			Category == SelectedStoreCategory;
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(
					TEXT("StoreCategory_%d"),
					Category),
				1700 + Category,
				CommonAtlasPath,
				bSelected
					? PageSelectedRect
					: PageNormalRect,
				CommonAtlasSize,
				FVector2D(260.0f, 48.0f),
				StoreCategoryLabels[Category],
				23,
				bSelected
					? FLinearColor::White
					: FLinearColor(0.72f, 0.72f, 0.70f, 1.0f),
				ESlateBrushDrawType::Box,
				FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
			CapturePoint(
				195.0f + Category * 260.0f,
				160.0f),
			FVector2D(260.0f, 48.0f),
			6);
	}

	URokUnityEconomyCatalogSubsystem* Catalog =
		GetEconomyCatalogSubsystem();
	TArray<FRokUnityEconomyItem> StoreItems;
	if (Catalog)
	{
		StoreItems = Catalog->GetShopItemsByCategory(
			StoreCategoryTypes[
				FMath::Clamp(
					SelectedStoreCategory,
					0,
					3)]);
	}
	constexpr int32 StoreItemsPerPage = 12;
	const int32 PageCount = FMath::Max(
		1,
		FMath::DivideAndRoundUp(
			StoreItems.Num(),
			StoreItemsPerPage));
	SelectedStorePage = FMath::Clamp(
		SelectedStorePage,
		0,
		PageCount - 1);
	const int32 FirstItemIndex =
		SelectedStorePage * StoreItemsPerPage;
	const int32 PageItemCount = FMath::Min(
		StoreItemsPerPage,
		StoreItems.Num() - FirstItemIndex);
	VisibleStoreItemIds.Reset();
	for (int32 Index = 0; Index < PageItemCount; ++Index)
	{
		VisibleStoreItemIds.Add(
			StoreItems[FirstItemIndex + Index].Id);
	}
	SelectedStoreItemIndex = FMath::Clamp(
		SelectedStoreItemIndex,
		0,
		FMath::Max(0, VisibleStoreItemIds.Num() - 1));

	auto ResolveQualityColor =
		[](int32 Quality)
		{
			switch (Quality)
			{
			case 2:
				return FLinearColor(0.14f, 0.34f, 0.58f, 1.0f);
			case 3:
				return FLinearColor(0.42f, 0.20f, 0.58f, 1.0f);
			case 4:
				return FLinearColor(0.72f, 0.39f, 0.12f, 1.0f);
			case 5:
				return FLinearColor(0.74f, 0.63f, 0.20f, 1.0f);
			default:
				return FLinearColor(0.28f, 0.31f, 0.29f, 1.0f);
			}
		};

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("StoreGridShade"),
			FLinearColor(0.10f, 0.11f, 0.10f, 0.98f)),
		CapturePoint(200.0f, 222.0f),
		FVector2D(670.0f, 505.0f),
		3);
	for (int32 LocalIndex = 0;
		LocalIndex < VisibleStoreItemIds.Num();
		++LocalIndex)
	{
		const FRokUnityEconomyItem& Item =
			StoreItems[FirstItemIndex + LocalIndex];
		const int32 Column = LocalIndex % 4;
		const int32 Row = LocalIndex / 4;
		const FVector2D Position(
			213.0f + Column * 162.0f,
			236.0f + Row * 155.0f);
		const bool bSelected =
			LocalIndex == SelectedStoreItemIndex;
		UCanvasPanel* Cell =
			WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(),
				*FString::Printf(
					TEXT("StoreCell_%d"),
					LocalIndex));
		PlaceOnCanvas(
			Cell,
			MakeSolidBorder(
				*FString::Printf(
					TEXT("StoreCellQuality_%d"),
					LocalIndex),
				ResolveQualityColor(Item.Quality)),
			FVector2D::ZeroVector,
			FVector2D(148.0f, 142.0f),
			0);
		if (bSelected)
		{
			PlaceOnCanvas(
				Cell,
				MakeAtlasBorder(
					*FString::Printf(
						TEXT("StoreCellSelected_%d"),
						LocalIndex),
					CommonAtlasPath,
					TaskRowRect,
					CommonAtlasSize,
					FVector2D(148.0f, 142.0f),
					TaskRowMargin),
				FVector2D::ZeroVector,
				FVector2D(148.0f, 142.0f),
				2);
		}
		UTextBlock* Name = MakeScreenText(
			*FString::Printf(
				TEXT("StoreCellName_%d"),
				LocalIndex),
			17,
			FLinearColor::White,
			true,
			ETextJustify::Center);
		Name->SetText(
			Item.DisplayNameZhCn.IsEmpty()
				? FText::Format(
					NSLOCTEXT(
						"RokStore",
						"FallbackItemName",
						"道具 {0}"),
					FText::AsNumber(Item.Id))
				: FText::FromString(
					Item.DisplayNameZhCn));
		Name->SetAutoWrapText(true);
		PlaceOnCanvas(
			Cell,
			Name,
			FVector2D(8.0f, 12.0f),
			FVector2D(132.0f, 72.0f),
			3);
		UTextBlock* Price = MakeScreenText(
			*FString::Printf(
				TEXT("StoreCellPrice_%d"),
				LocalIndex),
			17,
			FLinearColor(0.98f, 0.91f, 0.57f, 1.0f),
			true,
			ETextJustify::Center);
		Price->SetText(FText::Format(
			NSLOCTEXT(
				"RokStore",
				"PriceFormat",
				"宝石 {0}"),
			FText::AsNumber(Item.ShopPrice)));
		PlaceOnCanvas(
			Cell,
			Price,
			FVector2D(8.0f, 102.0f),
			FVector2D(132.0f, 30.0f),
			3);
		PlaceOnCanvas(
			Cell,
			MakeActionButton(
				*FString::Printf(
					TEXT("StoreCellHit_%d"),
					LocalIndex),
				1720 + LocalIndex,
				nullptr,
				FIntRect(),
				FIntPoint::ZeroValue,
				FVector2D(148.0f, 142.0f)),
			FVector2D::ZeroVector,
			FVector2D(148.0f, 142.0f),
			8);
		PlaceOnCanvas(
			ModeCanvas,
			Cell,
			CapturePoint(Position.X, Position.Y),
			FVector2D(148.0f, 142.0f),
			5);
	}

	FRokUnityEconomyItem SelectedItem;
	const bool bHasSelectedItem =
		Catalog
		&& VisibleStoreItemIds.IsValidIndex(
			SelectedStoreItemIndex)
		&& Catalog->FindItemById(
			VisibleStoreItemIds[SelectedStoreItemIndex],
			SelectedItem);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("StoreDetailShade"),
			FLinearColor(0.18f, 0.19f, 0.18f, 0.98f)),
		CapturePoint(890.0f, 222.0f),
		FVector2D(345.0f, 505.0f),
		4);
	UTextBlock* DetailName = MakeScreenText(
		TEXT("StoreDetailName"),
		24,
		FLinearColor(0.96f, 0.90f, 0.65f, 1.0f),
		true,
		ETextJustify::Center);
	DetailName->SetText(
		bHasSelectedItem
			? FText::FromString(
				SelectedItem.DisplayNameZhCn.IsEmpty()
					? FString::Printf(
						TEXT("道具 %d"),
						SelectedItem.Id)
					: SelectedItem.DisplayNameZhCn)
			: NSLOCTEXT(
				"RokStore",
				"NoItem",
				"当前分类无商品"));
	DetailName->SetAutoWrapText(true);
	PlaceOnCanvas(
		ModeCanvas,
		DetailName,
		CapturePoint(910.0f, 247.0f),
		FVector2D(305.0f, 68.0f),
		6);
	UTextBlock* DetailSource = MakeScreenText(
		TEXT("StoreDetailSource"),
		16,
		FLinearColor(0.74f, 0.74f, 0.71f, 1.0f),
		false,
		ETextJustify::Center);
	DetailSource->SetText(
		bHasSelectedItem
			? FText::Format(
				NSLOCTEXT(
					"RokStore",
					"SourceFormat",
					"Item {0}  ·  品质 {1}  ·  货币 {2}"),
				FText::AsNumber(SelectedItem.Id),
				FText::AsNumber(SelectedItem.Quality),
				FText::AsNumber(
					SelectedItem.ShopCurrencyTypeId))
			: FText::GetEmpty());
	PlaceOnCanvas(
		ModeCanvas,
		DetailSource,
		CapturePoint(905.0f, 322.0f),
		FVector2D(315.0f, 34.0f),
		6);
	UTextBlock* Description = MakeScreenText(
		TEXT("StoreDetailDescription"),
		17,
		FLinearColor::White,
		false,
		ETextJustify::Center);
	Description->SetText(
		bHasSelectedItem
			? FText::FromString(
				ResolveEconomyItemDescription(SelectedItem))
			: FText::GetEmpty());
	Description->SetAutoWrapText(true);
	PlaceOnCanvas(
		ModeCanvas,
		Description,
		CapturePoint(915.0f, 365.0f),
		FVector2D(295.0f, 95.0f),
		6);

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StoreQuantityMinus"),
			1750,
			CommonAtlasPath,
			ItemMinusRect,
			CommonAtlasSize,
			FVector2D(46.0f),
			FText::FromString(TEXT("−")),
			23),
		CapturePoint(912.0f, 480.0f),
		FVector2D(46.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("StoreQuantityShade"),
			FLinearColor(0.11f, 0.12f, 0.11f, 0.98f)),
		CapturePoint(966.0f, 482.0f),
		FVector2D(135.0f, 43.0f),
		6);
	UTextBlock* Quantity = MakeScreenText(
		TEXT("StoreQuantity"),
		22,
		FLinearColor::White,
		true,
		ETextJustify::Center);
	Quantity->SetText(FText::AsNumber(SelectedStoreQuantity));
	PlaceOnCanvas(
		ModeCanvas,
		Quantity,
		CapturePoint(966.0f, 487.0f),
		FVector2D(135.0f, 34.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StoreQuantityPlus"),
			1751,
			CommonAtlasPath,
			ItemPlusRect,
			CommonAtlasSize,
			FVector2D(46.0f),
			FText::FromString(TEXT("+")),
			23),
		CapturePoint(1109.0f, 480.0f),
		FVector2D(46.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StoreQuantityMax"),
			1752,
			CommonAtlasPath,
			MiniBlueButtonRect,
			CommonAtlasSize,
			FVector2D(67.0f, 46.0f),
			NSLOCTEXT("RokStore", "Max", "最大"),
			17),
		CapturePoint(1162.0f, 480.0f),
		FVector2D(67.0f, 46.0f),
		7);

	const int64 TotalPrice = bHasSelectedItem
		? static_cast<int64>(SelectedItem.ShopPrice)
			* SelectedStoreQuantity
		: 0;
	UTextBlock* Total = MakeScreenText(
		TEXT("StoreTotalPrice"),
		22,
		FLinearColor(0.98f, 0.91f, 0.57f, 1.0f),
		true,
		ETextJustify::Center);
	Total->SetText(FText::Format(
		NSLOCTEXT(
			"RokStore",
			"TotalPriceFormat",
			"总价：{0} 宝石"),
		FText::AsNumber(TotalPrice)));
	PlaceOnCanvas(
		ModeCanvas,
		Total,
		CapturePoint(910.0f, 550.0f),
		FVector2D(305.0f, 38.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StoreBuyButton"),
			1753,
			CommonAtlasPath,
			MediumGoldButtonRect,
			CommonAtlasSize,
			FVector2D(220.0f, 75.0f),
			NSLOCTEXT("RokStore", "Buy", "购买"),
			26,
			FLinearColor(0.25f, 0.13f, 0.02f, 1.0f),
			ESlateBrushDrawType::Box,
			FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		CapturePoint(952.0f, 618.0f),
		FVector2D(220.0f, 75.0f),
		8);

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StorePreviousPage"),
			1740,
			CommonAtlasPath,
			MiniBlueButtonRect,
			CommonAtlasSize,
			FVector2D(110.0f, 46.0f),
			NSLOCTEXT("RokStore", "Previous", "上一页"),
			17),
		CapturePoint(395.0f, 744.0f),
		FVector2D(110.0f, 46.0f),
		7);
	UTextBlock* Page = MakeScreenText(
		TEXT("StorePage"),
		18,
		FLinearColor::White,
		true,
		ETextJustify::Center);
	Page->SetText(FText::Format(
		NSLOCTEXT(
			"RokStore",
			"PageFormat",
			"{0}/{1}  ·  本类 {2} 项"),
		FText::AsNumber(SelectedStorePage + 1),
		FText::AsNumber(PageCount),
		FText::AsNumber(StoreItems.Num())));
	PlaceOnCanvas(
		ModeCanvas,
		Page,
		CapturePoint(510.0f, 752.0f),
		FVector2D(240.0f, 32.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(
			TEXT("StoreNextPage"),
			1741,
			CommonAtlasPath,
			MiniBlueButtonRect,
			CommonAtlasSize,
			FVector2D(110.0f, 46.0f),
			NSLOCTEXT("RokStore", "Next", "下一页"),
			17),
		CapturePoint(755.0f, 744.0f),
		FVector2D(110.0f, 46.0f),
		7);
}

#if !UE_BUILD_SHIPPING
AActor* URokUnityFeatureScreenWidget::GetFeatureContextActorForAudit() const
{
	return FeatureContextActor.Get();
}
#endif

void URokUnityFeatureScreenWidget::BuildTrainArmy()
{
	bTrainArmyQuantityEditable = false;
	TrainArmyAnimatedChildren.Reset();
	TrainArmyTipLayer = nullptr;
	const FText ArmyLabel = ResolveTroopTypeLabel(SelectedTrainArmyType);
	BuildSplitWindowFrame(
		FVector2D(133.5, 40.5),
		FVector2D(1173, 729),
		FText::Format(
			NSLOCTEXT("RokTrainArmy", "TitleFormat", "{0}训练"),
			ArmyLabel),
		CapturePoint(1230.0f, 77.0f));

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

	const int32 BuildingTypeId = SelectedTrainArmyType + 8;
	FRokUnityCatalogBuildingType BuildingType;
	const bool bHasBuildingType = GameplayCatalog
		&& GameplayCatalog->GetBuildingType(
			BuildingTypeId,
			BuildingType);
	const int32 TrainingCapacity = bHasBuildingType
		? BuildingType.GetTrainingCapacity(BuildingLevel)
		: 0;
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

	if (SelectedTroop) BuildTrainArmySharedDisplay(SelectedTroop->Id);
	if (bTrainArmyDetailVisible && SelectedTroop)
	{
		BuildTrainArmyDetail(SelectedTroop->Id);
		return;
	}
	const int32 TrainFirstChild = ModeCanvas->GetChildrenCount();
	UTextBlock* BuildingContext = MakeScreenText(
		TEXT("TrainArmyBuildingContext"),
		19,
		FLinearColor(0.82f, 0.81f, 0.75f, 1.0f),
		false,
		ETextJustify::Center);
	BuildingContext->SetText(FText::Format(
		NSLOCTEXT(
			"RokTrainArmy",
			"BuildingContextFormat",
			"{0}  ·  等级 {1}  ·  单队容量 {2}"),
		BuildingName,
		FText::AsNumber(BuildingLevel),
		TrainingCapacity > 0
			? FText::AsNumber(TrainingCapacity)
			: NSLOCTEXT(
				"RokTrainArmy",
				"CapacityUnavailable",
				"未载入")));
	PlaceOnCanvas(
		ModeCanvas,
		BuildingContext,
		CapturePoint(190.0f, 151.0f),
		FVector2D(1060.0f, 34.0f),
		5);

	for (int32 TierIndex = 0; TierIndex < 5; ++TierIndex)
	{
		const int32 Tier = TierIndex + 1;
		const FRokUnityCatalogTroop* TierTroop = Troops.FindByPredicate(
			[Tier](const FRokUnityCatalogTroop& Troop)
			{
				return Troop.Level == Tier;
			});
		const bool bSelected = SelectedTrainTier == Tier;
		const bool bNoStudyRequired = TierTroop && TierTroop->StudyId == 0;
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
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					*FString::Printf(TEXT("TrainArmyTierIcon_%d"), TierIndex),
					TroopHeadAtlasPath,
					TroopRect,
					TroopHeadAtlasSize,
					FVector2D(116.0f)),
				Position + FVector2D(8.0f),
				FVector2D(116.0f),
				6);
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
		TierLabel->SetText(bNoStudyRequired
			? FText::Format(
				NSLOCTEXT("RokTrainArmy", "TierFormat", "T{0}"),
				FText::AsNumber(Tier))
			: FText::Format(
				NSLOCTEXT("RokTrainArmy", "UnknownTierStudyFormat", "T{0}  科技待同步"),
				FText::AsNumber(Tier)));
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

		int32 BootstrapOwned = 0;
		if (RuntimeState)
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
		CostAndTime->SetText(FText::Format(
			NSLOCTEXT(
				"RokTrainArmy",
				"CostTimeFormat",
				"总消耗：{0}    训练时间：{1}\n本地启动快照拥有：{2}（仅迁移验收，不是服务器库存）"),
			FormatBuildCost(TotalCost),
			FormatBuildDuration(TotalSeconds),
			FText::AsNumber(BootstrapOwned)));
		CostAndTime->SetAutoWrapText(true);
		PlaceOnCanvas(
			ModeCanvas,
			CostAndTime,
			CapturePoint(425.0f, 505.0f),
			FVector2D(750.0f, 80.0f),
			7);
	}
	else
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
			NSLOCTEXT("RokTrainArmy", "StartTraining", "开始训练"),
			27,
			FLinearColor(0.25f, 0.13f, 0.02f, 1.0f),
			ESlateBrushDrawType::Box,
			FMargin(0.42f, 0.0f, 0.42f, 0.0f)),
		CapturePoint(905.0f, 622.0f),
		FVector2D(250.0f, 76.0f),
		9);
	bTrainArmyQuantityEditable = SelectedTroop && SelectedTroop->StudyId == 0 && TrainArmyBaseCapacity > 0;
	BuildTrainArmyQuantityControls();
	BuildTrainResourceCost();
	// ClickHead permits browsing every tier. Unknown research never grants
	// training eligibility: mirror the source hidden parameter/action subtree.
	if (SelectedTroop && SelectedTroop->StudyId != 0)
	{
		const FName HiddenControls[] = { TEXT("TrainArmyCostAndTime"), TEXT("TrainArmyMinus"),
			TEXT("TrainArmyQuantityShade"), TEXT("TrainArmyQuantity"), TEXT("TrainArmyPlus"),
			TEXT("TrainArmyMax"), TEXT("TrainArmyStart"), TEXT("TrainArmyQuantityInput"),
			TEXT("TrainArmyQuantitySlider"), TEXT("TrainArmyQuantitySliderBackground"), TEXT("TrainArmyQuantitySliderFill"), TEXT("TrainResourceCostRoot") };
		for (const FName& Name : HiddenControls)
			if (UWidget* Child = GetWidgetFromName(Name)) Child->SetVisibility(ESlateVisibility::Collapsed);
		auto* Status = MakeScreenText(TEXT("TrainArmyStudyStatus"), 20, FLinearColor(0.92f, 0.85f, 0.65f, 1.0f));
		Status->SetAutoWrapText(true);
		Status->SetText(NSLOCTEXT("RokTrainArmy", "StudyStatusUnknown",
			"当前兵阶需要科技解锁，解锁状态待同步。可查看兵种详情。"));
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
	AdvanceTrainArmyDetailAnimation(0.0f);
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
		|| ActiveMode != ERokUnityFeatureScreenMode::TrainArmy) return;
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
	if (!GetWidgetFromName(TEXT("TrainResourceCostRoot"))) return;
	auto* Catalog = GetGameplayCatalogSubsystem();
	const auto Troops = Catalog ? Catalog->GetTroopsForArmyType(SelectedTrainArmyType) : TArray<FRokUnityCatalogTroop>();
	const auto* Troop = Troops.FindByPredicate([this](const auto& Row) { return Row.Level == SelectedTrainTier; });
	if (!Troop) return;
	const int32 Base[] = { Troop->TrainCost.Food, Troop->TrainCost.Wood, Troop->TrainCost.Stone, Troop->TrainCost.Gold };
	int32 VisibleCount = 0;
	for (int32 Value : Base) if (Value > 0) ++VisibleCount;
	int32 VisibleIndex = 0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Cell = GetWidgetFromName(*FString::Printf(TEXT("TrainResourceCostCell_%d"), Index));
		if (!Cell) continue;
		Cell->SetVisibility(Base[Index] > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (Base[Index] <= 0) continue;
		const auto* RootSlot = Cast<UCanvasPanelSlot>(GetWidgetFromName(TEXT("TrainResourceCostRoot"))->Slot);
		const bool bSourceLayout = RootSlot && FMath::IsNearlyEqual(RootSlot->GetSize().X, 548.0);
		if (auto* CellSlot = Cast<UCanvasPanelSlot>(Cell->Slot)) CellSlot->SetPosition(FVector2D((bSourceLayout ? 0 : 101) + RokTrainResourceCost::CellX(VisibleCount, VisibleIndex++), bSourceLayout ? 13.95 : 0));
		if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("TrainResourceCostAmount_%d"), Index))))
			Text->SetText(FText::FromString(RokTrainResourceCost::Format(static_cast<int64>(Base[Index]) * SelectedTrainCount)));
	}
	if (auto* Time = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainResourceBaseTime"))))
	{
		const int64 Seconds = static_cast<int64>(Troop->TrainSeconds) * SelectedTrainCount;
		Time->SetText(FText::FromString(FString::Printf(TEXT("基础训练时间：%02lld:%02lld:%02lld  ·  加成、库存待同步"), Seconds / 3600, Seconds / 60 % 60, Seconds % 60)));
	}
}

void URokUnityFeatureScreenWidget::BuildTrainArmySharedDisplay(int32 TroopId)
{
	// ShowAttrDetail / OnDetailReturn keep pl_left and the window frame alive.
	FRokTrainArmyDetail Detail; FString Error;
	if (!RokTrainArmyDetail::Load(TroopId, Detail, Error)) { UE_LOG(LogTemp, Error, TEXT("%s"), *Error); return; }
	const TCHAR* TroopAtlas = *Detail.Attributes[0].Texture;
	const auto TroopAtlasSize = Detail.Attributes[0].AtlasSize;
	// Source left art stays fixed while the pl_spine mesh keeps its own idle clock.
	auto* LeftBack = MakeAtlasBorder(TEXT("TrainParameterLeftBacking"), CommonAtlasPath, SpriteRect(1003, 932, 400, 42),
		CommonAtlasSize, FVector2D(464.9, 656), FMargin(182.f / 400, 13.f / 42, 115.f / 400, 12.f / 42));
	PlaceOnCanvas(ModeCanvas, LeftBack, FVector2D(175.95, 99.9), FVector2D(464.9, 656), 3);
	LeftBack->SetRenderTransformPivot(FVector2D(.5, .5)); LeftBack->SetRenderScale(FVector2D(-1, 1));
	auto* LeftImage = MakeAtlasImage(TEXT("TrainParameterLeftArt"), TroopAtlas, SpriteRect(1, 1392, 447, 655), TroopAtlasSize, FVector2D(453.5, 656));
	PlaceOnCanvas(ModeCanvas, LeftImage, FVector2D(179.05, 99.8), FVector2D(453.5, 656), 4);
	LeftImage->SetRenderTransformPivot(FVector2D(.5, .5)); LeftImage->SetRenderScale(FVector2D(-1, 1));
	if (!TrainArmyAnimatedModel)
		TrainArmyAnimatedModel = WidgetTree->ConstructWidget<URokTrainArmyMeshWidget>(URokTrainArmyMeshWidget::StaticClass(), TEXT("TrainArmyAnimatedModel"));
	if (TrainArmyAnimatedModel->LoadArms(TroopId))
	{
		TrainArmyAnimatedModel->SetVisibility(ESlateVisibility::HitTestInvisible);
		PlaceOnCanvas(ModeCanvas, TrainArmyAnimatedModel, FVector2D::ZeroVector, FVector2D(1440, 810), 4);
		return;
	}
	// Missing source is a visible diagnostic, never a fabricated animated model.
	FIntRect PortraitRect;
	if (ResolveTroopHeadRect(SelectedTrainArmyType, SelectedTrainTier, PortraitRect))
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(bTrainArmyDetailVisible ? TEXT("TrainArmyDetailPortrait") : TEXT("TrainArmySelectedIcon"),
			TroopHeadAtlasPath, PortraitRect, TroopHeadAtlasSize, FVector2D(280)), FVector2D(270, 250), FVector2D(280), 6);
	auto* Preview = MakeScreenText(TEXT("TrainParameterStaticPreview"), 15, FLinearColor::White, false, ETextJustify::Center);
	Preview->SetText(FText::FromString(TEXT("模型加载失败 · 静态诊断预览"))); Preview->SetShadowOffset(FVector2D::ZeroVector);
	PlaceOnCanvas(ModeCanvas, Preview, FVector2D(270, 540), FVector2D(280, 30), 6);
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
			Label->SetText(FText::FromString(TEXT("—"))); auto Font = Label->GetFont(); Font.Size = 15; Label->SetFont(Font);
			Label->SetColorAndOpacity(FLinearColor::White); Label->SetShadowOffset(FVector2D::ZeroVector);
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
		auto Style = Input->GetWidgetStyle(); Style.TextStyle.Font.Size = 15; Input->SetWidgetStyle(Style);
	}
	Move(TEXT("TrainArmyQuantity"), InputPosition(), FVector2D(136.53, 40));
	if (auto* Quantity = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainArmyQuantity"))))
	{
		Quantity->SetText(FText::AsNumber(SelectedTrainCount)); auto Font = Quantity->GetFont(); Font.Size = 15; Quantity->SetFont(Font);
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
	Move(TEXT("TrainResourceBaseTime"), FVector2D(0, 73), FVector2D(548, 30));
	Move(TEXT("TrainArmyStart"), TrainPosition(), FVector2D(274, 84));
	Hide(TEXT("TrainArmyStartBackground"));
	if (auto* StartContent = Cast<UCanvasPanel>(GetWidgetFromName(TEXT("TrainArmyStartContent"))))
		PlaceOnCanvas(StartContent, MakeAtlasImage(TEXT("TrainParameterStartBlue"), CommonAtlasPath, SpriteRect(1490, 617, 274, 84), CommonAtlasSize, FVector2D(274, 84)), FVector2D::ZeroVector, FVector2D(274, 84), 0);
	Move(TEXT("TrainArmyStartLabel"), FVector2D(0, 23), FVector2D(274, 38));
	if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("TrainArmyStartLabel"))))
	{
		Label->SetText(FText::FromString(TEXT("训练"))); Label->SetColorAndOpacity(FLinearColor::White);
		auto Font = Label->GetFont(); Font.Size = 18; Label->SetFont(Font); Label->SetShadowOffset(FVector2D::ZeroVector);
	}
	Move(TEXT("TrainArmyStudyStatus"), FVector2D(677.9, 438.4), FVector2D(548, 70));
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

void URokUnityFeatureScreenWidget::BuildTrainArmyDetail(int32 TroopId)
{
	FRokTrainArmyDetail Detail; FString Error;
	if (!RokTrainArmyDetail::Load(TroopId, Detail, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		auto* Missing = MakeScreenText(TEXT("TrainArmyDetailMissing"), 22, FLinearColor::White);
		Missing->SetText(FText::FromString(TEXT("兵种详情配置未载入")));
		PlaceOnCanvas(ModeCanvas, Missing, CapturePoint(650, 300), FVector2D(540, 60), 10);
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
	TrainArmyTipLayer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TrainArmyDetailTipLayer"));
	PlaceOnCanvas(ModeCanvas, TrainArmyTipLayer, FVector2D::ZeroVector, CurrentExpandedDesignSize, 100);
	auto* Mask = MakeActionButton(TEXT("TrainArmyDetailTipClickAway"), 1952, nullptr, FIntRect(), FIntPoint(1, 1), CurrentExpandedDesignSize);
	PlaceOnCanvas(TrainArmyTipLayer, Mask, FVector2D::ZeroVector, CurrentExpandedDesignSize);
	const FString Text = bSkill ? Detail.Skills[Index].Tip : Detail.Attributes[Index].Tip;
	auto* Body = MakeScreenText(TEXT("TrainArmyDetailTipText"), 14, FLinearColor(0.137f, 0.137f, 0.125f, 1));
	Body->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18.0f * 72.0f / 96.0f));
	Body->SetShadowOffset(FVector2D::ZeroVector);
	Body->SetText(FText::FromString(Text)); Body->SetWrapTextAt(500); Body->SetAutoWrapText(true);
	// HelpTip.SetWidth is text width, plus Frame(40,40); offsets reference the
	// clicked button center and include the 12px arrow. Keep source atlas alpha.
	const TSharedRef<SWidget> BodySlate = Body->TakeWidget();
	Body->ForceLayoutPrepass();
	const float Height = Body->GetDesiredSize().Y + 40.0f;
	const float AnchorY = bSkill ? 137 + 511.25f + 42 : 137 + 321 - Detail.Attributes[Index].UnityY;
	const float AnchorX = bSkill ? 618 + RokTrainArmyDetail::SkillCellX(Detail.Skills.Num(), Index) + 50 : 930;
	const float Left = FMath::Clamp(AnchorX - 270.0f, 0.0f, 900.0f);
	const FVector2D Position = CapturePoint(Left, AnchorY - (bSkill ? 30 : 23) - 12 - Height);
	PlaceOnCanvas(TrainArmyTipLayer, MakeActionButton(TEXT("TrainArmyDetailTipInside"), 1953, nullptr,
		FIntRect(), FIntPoint(1, 1), FVector2D(540, Height)), Position, FVector2D(540, Height), 1);
	PlaceOnCanvas(TrainArmyTipLayer, MakeAtlasBorder(TEXT("TrainArmyDetailTipBackground"), CommonAtlasPath,
		SpriteRect(774, 276, 72, 72), CommonAtlasSize, FVector2D(540, Height),
		FMargin(21.0f / 72, 23.0f / 72, 21.0f / 72, 24.0f / 72)), Position, FVector2D(540, Height), 2);
	PlaceOnCanvas(TrainArmyTipLayer, Body, Position + FVector2D(20), FVector2D(500, Height - 40), 3);
	PlaceOnCanvas(TrainArmyTipLayer, MakeAtlasImage(TEXT("TrainArmyDetailTipArrow"), CommonAtlasPath,
		SpriteRect(120, 137, 15, 12), CommonAtlasSize, FVector2D(15, 12)), Position + FVector2D(AnchorX - Left - 7.5f, Height - 1.4f), FVector2D(15, 12), 3);
}

void URokUnityFeatureScreenWidget::BuildSpeedUp()
{
	BuildSplitWindowFrame(
		CapturePoint(146.0f, 73.0f),
		FVector2D(1148.0f, 754.0f),
		FText::Format(
			NSLOCTEXT("RokSpeedUp", "TitleFormat", "{0}加速"),
			ResolveSpeedUpQueueLabel(SpeedUpQueueType)),
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("SpeedUpContentShade"),
			FLinearColor(0.033f, 0.036f, 0.034f, 0.98f)),
		CapturePoint(162.0f, 145.0f),
		FVector2D(1116.0f, 626.0f),
		2);

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

	SpeedUpQueueDescriptionText = MakeScreenText(
		TEXT("SpeedUpQueueDescription"),
		20,
		bHasActiveQueue
			? FLinearColor(0.94f, 0.90f, 0.65f, 1.0f)
			: FLinearColor(0.78f, 0.78f, 0.74f, 1.0f),
		true,
		ETextJustify::Center);
	SpeedUpQueueDescriptionText->SetText(QueueDescription);
	SpeedUpQueueDescriptionText->SetAutoWrapText(true);
	PlaceOnCanvas(
		ModeCanvas,
		SpeedUpQueueDescriptionText,
		CapturePoint(195.0f, 158.0f),
		FVector2D(1050.0f, 55.0f),
		6);

	SpeedUpQueueProgress = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(),
		TEXT("SpeedUpQueueProgress"));
	SpeedUpQueueProgress->SetPercent(QueueProgress);
	SpeedUpQueueProgress->SetFillColorAndOpacity(
		bHasActiveQueue
			? FLinearColor(0.25f, 0.76f, 0.34f, 1.0f)
			: FLinearColor(0.25f, 0.28f, 0.26f, 1.0f));
	SpeedUpQueueProgress->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(
		ModeCanvas,
		SpeedUpQueueProgress,
		CapturePoint(245.0f, 220.0f),
		FVector2D(950.0f, 24.0f),
		6);
	SpeedUpQueueProgressText = MakeScreenText(
		TEXT("SpeedUpQueueProgressText"),
		16,
		FLinearColor::White,
		true,
		ETextJustify::Center);
	SpeedUpQueueProgressText->SetText(bHasActiveQueue
		? FText::Format(
			NSLOCTEXT("RokSpeedUp", "ProgressFormat", "已完成 {0}%"),
			FText::AsNumber(FMath::RoundToInt(QueueProgress * 100.0f)))
		: NSLOCTEXT("RokSpeedUp", "NoProgress", "无活动队列"));
	PlaceOnCanvas(
		ModeCanvas,
		SpeedUpQueueProgressText,
		CapturePoint(245.0f, 217.0f),
		FVector2D(950.0f, 28.0f),
		7);

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
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				*FString::Printf(TEXT("SpeedUpItemShade_%d"), LocalIndex),
				bSelected
					? FLinearColor(0.24f, 0.20f, 0.10f, 0.98f)
					: FLinearColor(0.075f, 0.08f, 0.07f, 0.98f)),
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
		const TCHAR* IconAtlasPath = nullptr;
		FIntRect IconRect;
		FIntPoint IconAtlasSize;
		if (ResolveEconomyItemIcon(
			Item,
			IconAtlasPath,
			IconRect,
			IconAtlasSize))
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
		Name->SetText(Item.DisplayNameZhCn.IsEmpty()
			? FText::Format(
				NSLOCTEXT("RokSpeedUp", "FallbackItemName", "加速道具 {0}"),
				FText::AsNumber(Item.Id))
			: FText::FromString(Item.DisplayNameZhCn));
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
		Details->SetText(FText::Format(
			NSLOCTEXT(
				"RokSpeedUp",
				"ItemDetailsFormat",
				"加速 {0}  ·  Item {1}\n库存：--（等待服务器背包）"),
			FormatBuildDuration(Item.SpeedDurationSeconds),
			FText::AsNumber(Item.Id)));
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
	UTextBlock* Page = MakeScreenText(
		TEXT("SpeedUpPage"),
		18,
		FLinearColor::White,
		true,
		ETextJustify::Center);
	Page->SetText(FText::Format(
		NSLOCTEXT(
			"RokSpeedUp",
			"PageFormat",
			"{0}/{1}  ·  {2} 项"),
		FText::AsNumber(SelectedSpeedUpPage + 1),
		FText::AsNumber(PageCount),
		FText::AsNumber(SpeedItems.Num())));
	PlaceOnCanvas(
		ModeCanvas,
		Page,
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
	if (!WorkerQueuePrimaryTitleText
		|| !WorkerQueueRuntimeStateText
		|| !WorkerQueuePrimaryProgress
		|| !WorkerQueuePrimaryProgressText)
	{
		return;
	}

	URokBuildingUpgradeSubsystem* UpgradeSubsystem = nullptr;
	if (UWorld* World = GetWorld())
	{
		UpgradeSubsystem =
			World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	}
	if (!UpgradeSubsystem)
	{
		WorkerQueueRuntimeStateText->SetText(NSLOCTEXT(
			"RokWorkerQueue",
			"RuntimeUnavailable",
			"建造运行态未加载，未显示推测的队列状态"));
		WorkerQueuePrimaryProgress->SetPercent(0.0f);
		WorkerQueuePrimaryProgressText->SetText(NSLOCTEXT(
			"RokWorkerQueue",
			"RuntimeUnavailableProgress",
			"队列不可用"));
		return;
	}

	AActor* ActiveUpgradeActor =
		UpgradeSubsystem->GetFirstActiveUpgradeActor();
	FRokBuildingState State;
	bool bHasActiveUpgrade = false;
	if (ActiveUpgradeActor)
	{
		State = UpgradeSubsystem
			->GetBuildingStateForActor(ActiveUpgradeActor);
		bHasActiveUpgrade = State.bUpgradeInProgress;
	}
	if (bHasActiveUpgrade != bWorkerQueueRenderedBusy)
	{
		RebuildModeContent();
		return;
	}

	if (!bHasActiveUpgrade)
	{
		WorkerQueuePrimaryTitleText->SetText(NSLOCTEXT(
			"RokWorkerQueue",
			"PrimaryTitle",
			"您的建造队列正处于空闲状态"));
		WorkerQueueRuntimeStateText->SetText(NSLOCTEXT(
			"RokWorkerQueue",
			"RuntimeIdleRefresh",
			"城主，您的建筑工人随时可以开工！"));
		WorkerQueuePrimaryProgress->SetPercent(0.0f);
		WorkerQueuePrimaryProgress->SetFillColorAndOpacity(
			FLinearColor(0.25f, 0.28f, 0.26f, 1.0f));
		WorkerQueuePrimaryProgressText->SetText(FText::GetEmpty());
		return;
	}

	const float Progress = FMath::Clamp(
		UpgradeSubsystem->GetUpgradeProgress(ActiveUpgradeActor),
		0.0f,
		1.0f);
	const int32 RemainingSeconds = FMath::Max(
		0,
		FMath::CeilToInt(
			UpgradeSubsystem->GetRemainingUpgradeSeconds(
				ActiveUpgradeActor)));
	WorkerQueuePrimaryTitleText->SetText(FText::Format(
		NSLOCTEXT(
			"RokWorkerQueue",
			"ActivePrimaryTitle",
			"等级 {1}  {0} 正在升级中…"),
		State.DisplayName,
		FText::AsNumber(FMath::Max(1, State.Level + 1))));
	WorkerQueueRuntimeStateText->SetText(NSLOCTEXT(
		"RokWorkerQueue",
		"RuntimeBusyRefresh",
		"城主，建筑工人们正热火朝天地辛勤工作着。"));
	WorkerQueuePrimaryProgress->SetPercent(Progress);
	WorkerQueuePrimaryProgress->SetFillColorAndOpacity(
		FLinearColor(0.25f, 0.76f, 0.34f, 1.0f));
	WorkerQueuePrimaryProgressText->SetText(FText::Format(
		NSLOCTEXT(
			"RokWorkerQueue",
			"PrimaryProgressFormat",
			"剩余 {0}  ·  已完成 {1}%"),
		FormatBuildDuration(RemainingSeconds),
		FText::AsNumber(FMath::RoundToInt(Progress * 100.0f))));
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

void URokUnityFeatureScreenWidget::BuildActivityCenter()
{
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

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok activity screen: tab=%d countdown_seconds=%d authority=local_preview."),
		SelectedActivityIndex,
		FMath::FloorToInt(ActivityPreviewSecondsRemaining));
}

void URokUnityFeatureScreenWidget::BuildActivityCalendarReadonly()
{
	URokUnityGameplayCatalogSubsystem* Catalog = GetGameInstance()
		? GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
	if (!Catalog || !Catalog->LoadActivityCalendarReadonly())
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
	static const FText Titles[] = {
		NSLOCTEXT("RokFeature", "ActivityCalendar", "活动日历"),
		NSLOCTEXT("RokFeature", "ActivityLordWarA", "城主之战"),
		NSLOCTEXT("RokFeature", "ActivityLordWarB", "城主之战"),
		NSLOCTEXT("RokFeature", "ActivityHarvestMoon", "丰收之月"),
		NSLOCTEXT("RokFeature", "ActivityLogin", "累计登陆")};
	static const FIntRect IconRects[] = {
		SpriteRect(497, 195, 60, 60),
		SpriteRect(431, 71, 60, 60),
		SpriteRect(431, 71, 60, 60),
		SpriteRect(369, 71, 60, 60),
		SpriteRect(369, 71, 60, 60)};
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
	PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("ActivityMenuIcon_%d"), Index), ActivityIconAtlasPath, IconRects[Index], ActivityIconAtlasSize, FVector2D(60.0f)), FVector2D(14.0f, 20.0f), FVector2D(60.0f), 2);
	UTextBlock* Label = MakeScreenText(*FString::Printf(TEXT("ActivityMenuLabel_%d"), Index), 20, bSelected ? FLinearColor(0.40f, 0.39f, 0.33f, 1.0f) : FLinearColor::White, true);
	Label->SetText(Titles[Index]);
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
	const FVector2D& ClosePosition)
{
	const float HalfWidth = Size.X * 0.5f;
	const FVector2D BodyPosition(Position.X, Position.Y + 52.0f);
	const FVector2D BodySize(HalfWidth, Size.Y - 62.0f);
	UCanvasPanel* BodyLeft = MakeAtlasBorder(TEXT("FeatureFrameBodyLeft"), FrameAtlasPath, WindowBodyRect, FrameAtlasSize, BodySize, WindowBodyMargin);
	PlaceOnCanvas(ModeCanvas, BodyLeft, BodyPosition, BodySize, 1);
	UCanvasPanel* BodyRight = MakeAtlasBorder(TEXT("FeatureFrameBodyRight"), FrameAtlasPath, WindowBodyRect, FrameAtlasSize, BodySize, WindowBodyMargin);
	BodyRight->SetRenderTransformPivot(FVector2D(0.5f));
	BodyRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, BodyRight, FVector2D(Position.X + HalfWidth, BodyPosition.Y), BodySize, 1);

	UCanvasPanel* HeaderLeft = MakeAtlasBorder(TEXT("FeatureFrameHeaderLeft"), FrameAtlasPath, WindowHeaderRect, FrameAtlasSize, FVector2D(HalfWidth, 60.0f), WindowHeaderMargin);
	PlaceOnCanvas(ModeCanvas, HeaderLeft, Position, FVector2D(HalfWidth, 60.0f), 5);
	UCanvasPanel* HeaderRight = MakeAtlasBorder(TEXT("FeatureFrameHeaderRight"), FrameAtlasPath, WindowHeaderRect, FrameAtlasSize, FVector2D(HalfWidth, 60.0f), WindowHeaderMargin);
	HeaderRight->SetRenderTransformPivot(FVector2D(0.5f));
	HeaderRight->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, HeaderRight, FVector2D(Position.X + HalfWidth, Position.Y), FVector2D(HalfWidth, 60.0f), 5);

	UTextBlock* TitleText = MakeScreenText(TEXT("FeatureFrameTitle"), 30, FLinearColor(0.96f, 0.94f, 0.88f, 1.0f), true, ETextJustify::Center);
	TitleText->SetText(Title);
	PlaceOnCanvas(ModeCanvas, TitleText, FVector2D(Position.X, Position.Y + 8.0f), FVector2D(Size.X, 42.0f), 7);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("FeatureFrameClose"), 1, CommonAtlasPath, CloseRect, CommonAtlasSize, FVector2D(69.0f)), ClosePosition, FVector2D(69.0f), 10);
}

void URokUnityFeatureScreenWidget::BuildResearch()
{
	// UI_Model_Window_Type1 under UI_Win_ResearchMain, measured in the
	// original 1440x810 UIRoot. Keep the source frame below the child tree.
	const FVector2D BodySize(570.0f, 698.1f);
	UCanvasPanel* Frame1 = MakeAtlasBorder(TEXT("ResearchFrame1"), FrameAtlasPath,
		WindowBodyRect, FrameAtlasSize, BodySize, WindowBodyMargin);
	PlaceOnCanvas(ModeCanvas, Frame1, FVector2D(166.5f, 94.5f), BodySize, 0);
	UCanvasPanel* Frame2 = MakeAtlasBorder(TEXT("ResearchFrame2"), FrameAtlasPath,
		WindowBodyRect, FrameAtlasSize, BodySize, WindowBodyMargin);
	Frame2->SetRenderTransformPivot(FVector2D(0.5f));
	Frame2->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, Frame2, FVector2D(703.5f, 94.5f), BodySize, 0);
	const FVector2D HeaderLeftSize(587.0f, 60.0f);
	const FVector2D HeaderRightSize(586.0f, 60.0f);
	PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("ResearchFrame3"), FrameAtlasPath,
		WindowHeaderRect, FrameAtlasSize, HeaderLeftSize, WindowHeaderMargin),
		FVector2D(133.5f, 68.5f), HeaderLeftSize, 2);
	UCanvasPanel* Frame4 = MakeAtlasBorder(TEXT("ResearchFrame4"), FrameAtlasPath,
		WindowHeaderRect, FrameAtlasSize, HeaderRightSize, WindowHeaderMargin);
	Frame4->SetRenderTransformPivot(FVector2D(0.5f));
	Frame4->SetRenderScale(FVector2D(-1.0f, 1.0f));
	PlaceOnCanvas(ModeCanvas, Frame4, FVector2D(720.5f, 68.5f), HeaderRightSize, 2);
	// The original frame Images consume raycasts over their RectTransforms.
	// Atlas pieces are decorative here, so preserve those hit regions before
	// enabling the full-screen ResearchMain touch-close mask behind this panel.
	auto AddFrameHit = [this](const TCHAR* Name, const FVector2D& Position,
		const FVector2D& Size)
	{
		UBorder* Hit = MakeSolidBorder(Name, FLinearColor::Transparent);
		Hit->SetVisibility(ESlateVisibility::Visible);
		PlaceOnCanvas(ModeCanvas, Hit, Position, Size, 1);
	};
	AddFrameHit(TEXT("ResearchMainBodyHitLeft"), FVector2D(166.5f, 94.5f), BodySize);
	AddFrameHit(TEXT("ResearchMainBodyHitRight"), FVector2D(703.5f, 94.5f), BodySize);
	AddFrameHit(TEXT("ResearchMainHeaderHitLeft"), FVector2D(133.5f, 68.5f), HeaderLeftSize);
	AddFrameHit(TEXT("ResearchMainHeaderHitRight"), FVector2D(720.5f, 68.5f), HeaderRightSize);
	ResearchScreenWidget = WidgetTree->ConstructWidget<URokResearchScreenWidget>(
		URokResearchScreenWidget::StaticClass(), TEXT("ResearchScreenRoot"));
	ResearchScreenWidget->OnCloseRequested.AddUObject(
		this, &URokUnityFeatureScreenWidget::RequestCloseScreen);
	ResearchScreenWidget->OnDetailVisibilityChanged.AddUObject(
		this, &URokUnityFeatureScreenWidget::HandleResearchDetailVisibilityChanged);
	ResearchScreenWidget->InitializeResearch(FeatureContextActor.Get());
	ResearchScreenWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ModeCanvas, ResearchScreenWidget, FVector2D::ZeroVector,
		FVector2D(1440.0f, 810.0f), 3);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("ResearchCloseButton"), 1,
		CommonAtlasPath, CloseRect, CommonAtlasSize, FVector2D(69.0f)),
		FVector2D(1218.5f, 63.0f), FVector2D(69.0f), 2);
}
void URokUnityFeatureScreenWidget::HandleResearchDetailVisibilityChanged(bool bVisible)
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Research || !WidgetTree) return;
	// UIManager.ShowLastMask keeps only the top PopWin mask active.
	if (UWidget* Dimmer = WidgetTree->FindWidget(TEXT("FeatureScreenDimmer")))
		Dimmer->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

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
		if (UTextBlock* TabLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TabLabelName)))
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
	const bool bLocalTabHasMail = bLocalMailUnreadFixtureState
		&& MailLocalUnreadState.FirstIndexForTab(SelectedMailTab) != INDEX_NONE;
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
		// The parent legacy pulse and child SkeletonGraphic have different clocks.
		// Reuse the child's source instance across row selection and tab rebuilds.
		auto& Mesh = MailAttachmentMeshes.FindOrAdd(Index);
		if (!Mesh)
		{
			Mesh = WidgetTree->ConstructWidget<URokMailAttachmentMeshWidget>(URokMailAttachmentMeshWidget::StaticClass(),
				*FString::Printf(TEXT("MailAttachmentMesh_%d"), Index));
			Mesh->LoadSource(); // Missing source is reported by the strict loader.
		}
		Mesh->RemoveFromParent();
		Mesh->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* Pulse = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("MailAttachmentPulse_%d"), Index));
		Pulse->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PlaceOnCanvas(Pulse, Mesh, FVector2D::ZeroVector, FVector2D(28.0f, 25.0f), 1);
		PlaceOnCanvas(Row, Pulse, FVector2D(392.32f, 31.43f), FVector2D(28.0f, 25.0f), 4);
		MailAttachmentPulse.Register(Index, Pulse);
	}
	PlaceOnCanvas(Row, MakeActionButton(*FString::Printf(TEXT("MailRowHit_%d"), Index), 220 + Index, nullptr, FIntRect(), FIntPoint(1, 1), FVector2D(457.0f, 114.24f)), FVector2D::ZeroVector, FVector2D(457.0f, 114.24f), 10);
	return Row;
}

void URokUnityFeatureScreenWidget::RefreshTaskMainProjection()
{
	if (ActiveMode != ERokUnityFeatureScreenMode::Tasks || SelectedTaskPage != 1
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

void URokUnityFeatureScreenWidget::BuildTaskChapter()
{
	// The source quest viewport is 737 x 468.5 with 740 x 104 zero-spacing rows.
	// The group chooser is a local config browser: Unity's live current chapter
	// depends on account state that is unavailable in this read-only screen.
	const FVector2D ChapterSize(1088.0f, 664.0f);
	UCanvasPanel* ChapterRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TaskChapterRoot"));
	PlaceOnCanvas(ModeCanvas, ChapterRoot, CapturePoint(176.0f, 136.0f), ChapterSize, 5);
	RenderedTaskChapterTaskIds.Reset();
	RenderedTaskChapterTaskRewardPackageIds.Reset();
	RenderedTaskChapterAggregateRewardPackageIds.Reset();

	for (int32 GroupId = 1; GroupId <= 7; ++GroupId)
	{
		const bool bSelected = GroupId == SelectedTaskChapterGroupId;
		const FVector2D Position(28.0f + (GroupId - 1) * 105.0f, 31.0f);
		PlaceOnCanvas(ChapterRoot,
			MakeActionButton(*FString::Printf(TEXT("TaskChapterGroupButton_%d"), GroupId),
				339 + GroupId, CommonAtlasPath, CommonPanelRect, CommonAtlasSize,
				FVector2D(99.0f, 43.0f), FText::Format(NSLOCTEXT("RokFeature", "TaskChapterGroupButton", "第{0}组"), FText::AsNumber(GroupId)),
				16, bSelected ? FLinearColor::White : FLinearColor(0.78f, 0.74f, 0.66f, 1.0f),
				ESlateBrushDrawType::Box, CommonPanelMargin),
			Position, FVector2D(99.0f, 43.0f), 2);
	}

	FRokTaskChapterPreviewRow Chapter;
	const bool bChapterFound = RokTaskChapterConfigPreviewData::FindChapterById(
		TaskChapterGroups, SelectedTaskChapterGroupId, Chapter);
	bSelectedTaskChapterMetadataKnown = bChapterFound && Chapter.bMetadataKnown;
	UTextBlock* GroupTitle = MakeScreenText(TEXT("TaskChapterGroupTitle"), 23,
		FLinearColor::FromSRGBColor(FColor(230, 217, 180)), true);
	GroupTitle->SetText(bChapterFound ? FText::FromString(Chapter.DisplayTitle())
		: NSLOCTEXT("RokFeature", "TaskChapterUnavailable", "章节配置不可用"));
	PlaceOnCanvas(ChapterRoot, GroupTitle, FVector2D(27.0f, 105.0f), FVector2D(720.0f, 39.0f), 2);

	UTextBlock* Notice = MakeScreenText(TEXT("TaskChapterMetaNotice"), 15,
		FLinearColor(0.78f, 0.75f, 0.67f, 1.0f));
	Notice->SetText(!TaskChapterLoadError.IsEmpty()
		? FText::FromString(TEXT("章节配置暂不可浏览。"))
		: !TaskChapterRewardLoadError.IsEmpty()
			? NSLOCTEXT("RokFeature", "TaskChapterRewardUnavailable", "奖励配置暂不可浏览；章节任务配置仍可查看。")
		: !bSelectedTaskChapterMetadataKnown
			? NSLOCTEXT("RokFeature", "TaskChapterMissingMetadata", "本组章节标题资料缺失；仅展示任务配置，账号进度未同步。")
			: NSLOCTEXT("RokFeature", "TaskChapterConfigOnly", "本地配置浏览 · 账号章节进度与领取状态未同步"));
	PlaceOnCanvas(ChapterRoot, Notice, FVector2D(27.0f, 151.0f), FVector2D(1010.0f, 29.0f), 2);
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

	// The chapter-title reward list is separate from the per-task rewards.
	// Chapter 7 has no source aggregate metadata, so it gets no synthetic cells.
	FRokTaskChapterRewardGroup Aggregate;
	if (RokTaskChapterRewardPreviewData::FindForChapter(
		TaskChapterRewardGroups, SelectedTaskChapterGroupId, Aggregate))
	{
		const int32 Count = Aggregate.Rows.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FRokTaskChapterRewardPreviewRow& Reward = Aggregate.Rows[Index];
			const FString Prefix = FString::Printf(TEXT("TaskChapterAggregateReward_%d_%d"),
				SelectedTaskChapterGroupId, Index);
			UCanvasPanel* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *Prefix);
			Cell->SetVisibility(ESlateVisibility::HitTestInvisible);
			// UI_Item_Bag/UI_Model_Item is 134 square under items scale .42.
			// ListView spacing 30 at the same scale gives a 68.88-pixel pitch.
			const FVector2D CellSize(56.28f, 56.28f);
			constexpr float CellPitch = 68.88f;
			PlaceOnCanvas(ChapterRoot, Cell,
				FVector2D(1040.0f - (Count - 1 - Index) * CellPitch - CellSize.X, 96.0f), CellSize, 3);
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
			RenderedTaskChapterAggregateRewardPackageIds.Add(Reward.PackageRowId);
		}
	}
	else if (bChapterFound && !bSelectedTaskChapterMetadataKnown)
	{
		UTextBlock* Missing = MakeScreenText(TEXT("TaskChapterAggregateRewardUnavailable"), 14,
			FLinearColor::FromSRGBColor(FColor(164, 157, 146)));
		Missing->SetText(NSLOCTEXT("RokFeature", "TaskChapterAggregateRewardMissing", "本章奖励资料缺失"));
		PlaceOnCanvas(ChapterRoot, Missing, FVector2D(779.0f, 105.0f), FVector2D(263.0f, 35.0f), 3);
	}

	UScrollBox* ChapterScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TaskChapterScroll"));
	ChapterScroll->SetOrientation(Orient_Vertical);
	ChapterScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	ChapterScroll->SetAllowOverscroll(false);
	ChapterScroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(ChapterRoot, ChapterScroll, FVector2D(19.23f, 196.2f), FVector2D(737.0f, 468.5f), 3);
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
		UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("TaskChapterTaskRow_%d"), TaskId));
		PlaceOnCanvas(Row, MakeAtlasBorder(*FString::Printf(TEXT("TaskChapterTaskBackground_%d"), TaskId),
			CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(740.0f, 100.0f), CommonPanelMargin),
			FVector2D::ZeroVector, FVector2D(740.0f, 100.0f), 0);

		UTextBlock* Title = MakeScreenText(*FString::Printf(TEXT("TaskChapterTaskTitle_%d"), TaskId),
			17, FLinearColor::FromSRGBColor(FColor(230, 217, 180)), true);
		Title->SetText(FText::FromString(Task.Title.ZhCn));
		Title->SetAutoWrapText(true);
		// Configuration-only title occupies the source account-action column.
		PlaceOnCanvas(Row, Title, FVector2D(14.0f, 16.0f), FVector2D(180.0f, 69.0f), 1);

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
		UTextBlock* DescriptionText = MakeScreenText(*FString::Printf(TEXT("TaskChapterTaskDescription_%d"), TaskId),
			17, FLinearColor::White, false, ETextJustify::Right);
		DescriptionText->SetText(FText::FromString(Description));
		DescriptionText->SetAutoWrapText(true);
		// Unity lbl_itemName presents TaskChapter.desc with live progress. This
		// preview keeps only the source description and leaves progress unknown.
		PlaceOnCanvas(Row, DescriptionText, FVector2D(200.7f, 8.4f), FVector2D(520.0f, 37.9f), 1);

		FRokTaskChapterRewardGroup RewardGroup;
		if (RokTaskChapterRewardPreviewData::FindForTask(TaskChapterRewardGroups, TaskId, RewardGroup))
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

void URokUnityFeatureScreenWidget::BuildTasks()
{
	RenderedTaskChapterTaskIds.Reset();
	RenderedTaskChapterTaskRewardPackageIds.Reset();
	RenderedTaskChapterAggregateRewardPackageIds.Reset();
	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f), NSLOCTEXT("RokFeature", "TaskTitle", "任务"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("TaskContentShade"), FLinearColor(0.065f, 0.07f, 0.07f, 0.98f)), CapturePoint(176.0f, 136.0f), FVector2D(1088.0f, 664.0f), 2);
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

	static const FIntRect SelectedIcons[] = {SpriteRect(1, 313, 60, 60), SpriteRect(1, 437, 60, 60), SpriteRect(63, 499, 60, 60)};
	static const FIntRect DarkIcons[] = {SpriteRect(1, 375, 60, 60), SpriteRect(1, 499, 60, 60), SpriteRect(63, 561, 60, 60)};
	static const FText TabLabels[] = {
		NSLOCTEXT("RokFeature", "TaskChapterTab", "章节"),
		NSLOCTEXT("RokFeature", "TaskMainTab", "主线"),
		NSLOCTEXT("RokFeature", "TaskDailyTab", "每日")};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const bool bSelected = Index == SelectedTaskPage;
		const FVector2D TabRoot = CapturePoint(1258.0f, 212.0f + Index * 113.0f);
		const FVector2D TabVisualPosition = TabRoot + FVector2D(bSelected ? 0.0f : 24.0f, 0.0f);
		const FVector2D TabVisualSize(bSelected ? 145.0f : 121.0f, 103.0f);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("TaskPage_%d"), Index), 300 + Index, CommonAtlasPath, bSelected ? SideTabSelectedRect : SideTabDarkRect, CommonAtlasSize, TabVisualSize, FText::GetEmpty(), 20, FLinearColor::White, ESlateBrushDrawType::Image), TabVisualPosition, TabVisualSize, 7);
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("TaskPageIcon_%d"), Index), PageAtlasPath, bSelected ? SelectedIcons[Index] : DarkIcons[Index], PageAtlasSize, FVector2D(60.0f)), TabVisualPosition + FVector2D(31.0f, 20.0f), FVector2D(60.0f), 8);
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

	if (SelectedTaskPage == 0)
	{
		BuildTaskChapter();
		return;
	}
	if (SelectedTaskPage != 1)
	{
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
				const FString DisplayName = Task.Name.IsEmpty() || Task.Name.Contains(TEXT("\uFFFD"))
					? TEXT("标题配置文本不可用") : Task.Name;
				Title->SetText(FText::FromString(FString::Printf(TEXT("#%d · %s"), TaskId, *DisplayName)));
				PlaceOnCanvas(Row, Title, FVector2D(170.0f, 15.0f), FVector2D(820.0f, 29.0f), 3);
				UTextBlock* Description = MakeScreenText(*FString::Printf(TEXT("TaskDailyCatalogDescription_%d"), TaskId),
					19, FLinearColor(0.93f, 0.93f, 0.91f, 1.0f));
				const FString DisplayDescription = Task.Desc.IsEmpty() || Task.Desc.Contains(TEXT("\uFFFD"))
					? TEXT("描述配置文本不可用")
					: ResolveTaskDescription(Task.Desc, Task.Param1, Task.Param2, Task.Require);
				Description->SetText(FText::FromString(DisplayDescription));
				PlaceOnCanvas(Row, Description, FVector2D(170.0f, 45.0f), FVector2D(820.0f, 27.0f), 3);
				UTextBlock* Target = MakeScreenText(*FString::Printf(TEXT("TaskDailyCatalogTarget_%d"), TaskId),
					17, FLinearColor(0.72f, 0.77f, 0.77f, 1.0f));
				Target->SetText(FText::FromString(FString::Printf(
					TEXT("目标配置：类型 %d · 参数 %d/%d · 需求 %d"),
					Task.Type, Task.Param1, Task.Param2, Task.Require)));
				PlaceOnCanvas(Row, Target, FVector2D(170.0f, 76.0f), FVector2D(820.0f, 24.0f), 3);
				UTextBlock* Reward = MakeScreenText(*FString::Printf(TEXT("TaskDailyCatalogReward_%d"), TaskId),
					17, FLinearColor(0.74f, 0.72f, 0.66f, 1.0f));
				Reward->SetText(FText::FromString(FString::Printf(
					TEXT("奖励配置组 #%d · 内容及可领取状态未验证"), Task.Reward)));
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

	UScrollBox* TaskScroll = WidgetTree->ConstructWidget<UScrollBox>(
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

	auto AddTaskTag = [this, TaskScroll](
		const FName& ContainerName,
		const FName& TextName,
		const FText& Label,
		const FLinearColor& Color)
	{
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
	auto AddTaskRow = [this, TaskScroll, &RewardPreviewByGroup, &UnavailableRewardGroups](
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
		if (RewardGroupId > 0 && !RewardPreviewByGroup.Contains(RewardGroupId)
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
		const TArray<FRokTaskRewardPreviewRow>* PreviewRows = RewardPreviewByGroup.Find(RewardGroupId);
		const TArray<FRokTaskRewardPreviewRow> EmptyPreviewRows;
		TaskScroll->AddChild(WrapFixedSize(
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
				PreviewRows ? *PreviewRows : EmptyPreviewRows),
			*FString::Printf(TEXT("TaskRowSize_%d"), RowIndex),
			FVector2D(1088.0f, 150.0f)));
	};

	AddTaskTag(
		TEXT("TaskMainTag"),
		TEXT("TaskMainHeader"),
		NSLOCTEXT("RokFeature", "MainTaskHeader", "主线任务"),
		FLinearColor(0.83f, 0.79f, 0.65f, 1.0f));

	const FRokUnityRuntimeTaskState Task = RuntimeState
		? RuntimeState->GetActiveTask()
		: FRokUnityRuntimeTaskState();
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
	else
	{
		UTextBlock* EmptyMain = MakeScreenText(TEXT("TaskMainEmpty"), 22, FLinearColor(0.72f, 0.72f, 0.7f, 1.0f), false, ETextJustify::Center);
		EmptyMain->SetText(NSLOCTEXT("RokFeature", "TaskMainEmpty", "暂无主线任务"));
		TaskScroll->AddChild(WrapFixedSize(EmptyMain, TEXT("TaskMainEmptySize"), FVector2D(1088.0f, 90.0f)));
	}
	if (bTaskTestFixtureState && Task.TaskId > 0)
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
		if (SideCandidates.Num() == 0)
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

UCanvasPanel* URokUnityFeatureScreenWidget::MakeTaskRow(
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
	const TArray<FRokTaskRewardPreviewRow>& RewardPreviewRows)
{
	UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("TaskRow_%d"), RowIndex));
	Row->SetClipping(EWidgetClipping::ClipToBounds);
	PlaceOnCanvas(Row, MakeAtlasBorder(*FString::Printf(TEXT("TaskRowBackground_%d"), RowIndex), CommonAtlasPath, TaskRowRect, CommonAtlasSize, FVector2D(1026.0f, 140.0f), TaskRowMargin), FVector2D(31.0f, 2.0f), FVector2D(1026.0f, 140.0f));
	PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("TaskIconQuality_%d"), RowIndex), Item0AtlasPath, ItemQualityLegendaryRect, Item0AtlasSize, FVector2D(100.0f)), FVector2D(60.0f, 22.0f), FVector2D(100.0f), 2);
	PlaceOnCanvas(Row, MakeAtlasImage(*FString::Printf(TEXT("TaskIcon_%d"), RowIndex), TaskIconAtlasPath, TaskIconRect, TaskIconAtlasSize, FVector2D(90.0f, 98.0f)), FVector2D(65.0f, 23.0f), FVector2D(90.0f, 98.0f), 3);
	UTextBlock* TitleText = MakeScreenText(*FString::Printf(TEXT("TaskTitle_%d"), RowIndex), 24, FLinearColor(0.87f, 0.78f, 0.49f, 1.0f), false);
	TitleText->SetText(Title);
	PlaceOnCanvas(Row, TitleText, FVector2D(175.0f, 18.0f), FVector2D(623.0f, 28.0f), 3);
	UTextBlock* DescriptionText = MakeScreenText(*FString::Printf(TEXT("TaskDescription_%d"), RowIndex), 20, FLinearColor(0.93f, 0.93f, 0.91f, 1.0f));
	DescriptionText->SetText(bProgressKnown
		? FText::Format(
			NSLOCTEXT("RokFeature", "TaskProgressFormat", "{0} ({1}/{2})"),
			Description,
			FText::AsNumber(Current),
			FText::AsNumber(Target))
		: Description);
	PlaceOnCanvas(Row, DescriptionText, FVector2D(175.0f, 45.0f), FVector2D(632.0f, 28.0f), 3);
	PlaceOnCanvas(Row, MakeAtlasBorder(*FString::Printf(TEXT("TaskRewardStrip_%d"), RowIndex), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(664.0f, 43.0f), CommonPanelMargin), FVector2D(167.0f, 79.0f), FVector2D(664.0f, 43.0f), 2);
	UTextBlock* RewardLabel = MakeScreenText(*FString::Printf(TEXT("TaskRewardLabel_%d"), RowIndex), 17, FLinearColor(0.74f, 0.72f, 0.66f, 1.0f));
	RewardLabel->SetText(NSLOCTEXT("RokFeature", "TaskReward", "奖励"));
	PlaceOnCanvas(Row, RewardLabel, FVector2D(180.0f, 86.0f), FVector2D(79.0f, 28.0f), 4);

	if (!RewardPreviewRows.IsEmpty())
	{
		for (int32 Index = 0; Index < RewardPreviewRows.Num(); ++Index)
		{
			const FRokTaskRewardPreviewRow& Preview = RewardPreviewRows[Index];
			const FString BaseName = FString::Printf(TEXT("TaskRewardPreview_%d_%d"), RowIndex, Index);
			UCanvasPanel* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *BaseName);
			const FText Quantity = FText::AsNumber(Preview.Quantity);
			Cell->SetToolTipText(FText::Format(
				NSLOCTEXT("RokFeature", "TaskRewardPreviewTooltip", "{0} ×{1} · 任务奖励预览，服务器状态待同步"),
				FText::FromString(Preview.DisplayName), Quantity));
			PlaceOnCanvas(Cell, MakeAtlasImage(*(BaseName + TEXT("Icon")), CommonAtlasPath,
				Preview.IconRect, Preview.IconAtlasSize, FVector2D(34.0f)), FVector2D::ZeroVector, FVector2D(34.0f), 1);
			UTextBlock* Count = MakeScreenText(*(BaseName + TEXT("Count")), 17,
				FLinearColor(0.95f, 0.9f, 0.75f, 1.0f), false);
			Count->SetText(Quantity);
			PlaceOnCanvas(Cell, Count, FVector2D(38.0f, 3.0f), FVector2D(92.0f, 27.0f), 2);
			PlaceOnCanvas(Row, Cell, FVector2D(268.0f + Index * 166.0f, 83.0f), FVector2D(138.0f, 36.0f), 5);
		}
	}
	else if (RewardGroupId > 0)
	{
		UTextBlock* Missing = MakeScreenText(*FString::Printf(TEXT("TaskRewardUnavailable_%d"), RowIndex), 16,
			FLinearColor(0.74f, 0.72f, 0.66f, 1.0f));
		Missing->SetText(NSLOCTEXT("RokFeature", "TaskRewardPreviewUnavailable", "奖励预览暂无来源数据"));
		PlaceOnCanvas(Row, Missing, FVector2D(269.0f, 86.0f), FVector2D(360.0f, 28.0f), 4);
	}

	const bool bCanShowClaim = bProgressKnown && bComplete;
	const FIntRect ButtonRect = bCanShowClaim ? MiniGreenButtonRect : MiniBlueButtonRect;
	const FMargin ButtonMargin = bCanShowClaim ? MiniGreenMargin : MiniBlueMargin;
	const FText ButtonLabel = bComplete
			? NSLOCTEXT("RokFeature", "TaskClaimReward", "领取奖励")
			: NSLOCTEXT("RokFeature", "TaskGo", "前往");
	PlaceOnCanvas(
		Row,
		MakeActionButton(
			*FString::Printf(TEXT("TaskAction_%d"), RowIndex),
			320 + RowIndex,
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

void URokUnityFeatureScreenWidget::BuildCommander()
{
	// UI_IF_Captain is a true full-screen view. CapturePoint maps the Unity
	// 1440x900 Expand capture onto the centred 1440x810 design canvas.
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

	const TArray<FRokUnityRuntimeHeroState> RuntimeHeroes = RuntimeState
		? RuntimeState->GetStarterHeroes()
		: TArray<FRokUnityRuntimeHeroState>();
	TSet<int32> PreviewOwnedHeroIds;
	for (const FRokUnityRuntimeHeroState& RuntimeHero : RuntimeHeroes)
	{
		PreviewOwnedHeroIds.Add(RuntimeHero.HeroId);
	}

	TArray<int32> CandidateHeroIds;
	if (GameplayCatalog)
	{
		for (const int32 HeroId : GameplayCatalog->GetHeroIds())
		{
			FRokUnityCatalogHero Hero;
			if (!GameplayCatalog->GetHero(HeroId, Hero))
			{
				continue;
			}
			if (Hero.ListDisplay == 1)
			{
				continue;
			}
			CandidateHeroIds.Add(HeroId);
		}
	}
	else
	{
		for (const FCommanderHeadSource& Source : CommanderHeadSources)
		{
			CandidateHeroIds.Add(Source.HeroId);
		}
	}
	const auto FindRuntimeHero =
		[&RuntimeHeroes](int32 HeroId) -> const FRokUnityRuntimeHeroState*
		{
			return RuntimeHeroes.FindByPredicate(
				[HeroId](const FRokUnityRuntimeHeroState& RuntimeHero)
				{
					return RuntimeHero.HeroId == HeroId;
				});
		};
	const auto ResolveCommanderSortValue =
		[this, GameplayCatalog, &FindRuntimeHero](int32 HeroId) -> int64
		{
			FRokUnityCatalogHero Hero;
			const bool bHasHero = GameplayCatalog
				&& GameplayCatalog->GetHero(HeroId, Hero);
			const FRokUnityRuntimeHeroState* RuntimeHero =
				FindRuntimeHero(HeroId);
			switch (SelectedCommanderSortType)
			{
			case 1:
				return RuntimeHero
					? RuntimeHero->Star
					: (bHasHero ? Hero.InitStar : 0);
			case 2:
				return RuntimeHero ? RuntimeHero->Level : 1;
			case 3:
				return RuntimeHero
					? RuntimeHero->Score
					: (bHasHero ? Hero.BasePower : 0);
			default:
				return bHasHero ? Hero.Rare : 0;
			}
		};
	CandidateHeroIds.Sort(
		[GameplayCatalog,
			&PreviewOwnedHeroIds,
			&ResolveCommanderSortValue](int32 Left, int32 Right)
		{
			const bool bLeftOwned = PreviewOwnedHeroIds.Contains(Left);
			const bool bRightOwned = PreviewOwnedHeroIds.Contains(Right);
			if (bLeftOwned != bRightOwned)
			{
				return bLeftOwned;
			}
			const int64 LeftSortValue = ResolveCommanderSortValue(Left);
			const int64 RightSortValue = ResolveCommanderSortValue(Right);
			if (LeftSortValue != RightSortValue)
			{
				return LeftSortValue > RightSortValue;
			}
			if (GameplayCatalog)
			{
				FRokUnityCatalogHero LeftHero;
				FRokUnityCatalogHero RightHero;
				if (GameplayCatalog->GetHero(Left, LeftHero)
					&& GameplayCatalog->GetHero(Right, RightHero)
					&& LeftHero.Rare != RightHero.Rare)
				{
					return LeftHero.Rare > RightHero.Rare;
				}
			}
			return Left < Right;
		});
	if (CandidateHeroIds.IsEmpty())
	{
		if (GameplayCatalog)
		{
			for (const int32 HeroId : GameplayCatalog->GetHeroIds())
			{
				FRokUnityCatalogHero Hero;
				if (GameplayCatalog->GetHero(HeroId, Hero)
					&& Hero.ListDisplay != 1)
				{
					CandidateHeroIds.Add(HeroId);
				}
			}
		}
		else
		{
			for (const FCommanderHeadSource& Source : CommanderHeadSources)
			{
				CandidateHeroIds.Add(Source.HeroId);
			}
		}
	}
	VisibleCommanderIds = CandidateHeroIds;
	if (!VisibleCommanderIds.Contains(SelectedCommanderId)
		&& !VisibleCommanderIds.IsEmpty())
	{
		SelectedCommanderId = VisibleCommanderIds[0];
		SelectedCommanderSkillIndex = 0;
		SelectedCommanderSkillDetailIndex = INDEX_NONE;
		SelectedCommanderTalentIndex = INDEX_NONE;
	}

	FRokUnityCatalogHero SelectedHero;
	const bool bHasSelectedHero = GameplayCatalog
		&& GameplayCatalog->GetHero(
			SelectedCommanderId,
			SelectedHero);
	const FRokUnityRuntimeHeroState* SelectedRuntimeHero =
		RuntimeHeroes.FindByPredicate(
			[this](const FRokUnityRuntimeHeroState& RuntimeHero)
			{
				return RuntimeHero.HeroId == SelectedCommanderId;
			});
	const bool bSelectedOwned = SelectedRuntimeHero != nullptr;
	const int32 SelectedStar = SelectedRuntimeHero
		? SelectedRuntimeHero->Star
		: (bHasSelectedHero ? SelectedHero.InitStar : 1);
	const int32 SelectedMaxStar = SelectedRuntimeHero
		? SelectedRuntimeHero->MaxStar
		: (bHasSelectedHero ? SelectedHero.MaxStar : 6);
	const int32 SelectedPower = SelectedRuntimeHero
		? SelectedRuntimeHero->Score
		: (bHasSelectedHero ? SelectedHero.BasePower : 0);
	const int32 SelectedLevel = SelectedRuntimeHero
		? SelectedRuntimeHero->Level
		: 1;
	const FString SelectedNameString = bHasSelectedHero
		? (!SelectedHero.DisplayNameZhCn.IsEmpty()
			? SelectedHero.DisplayNameZhCn
			: SelectedHero.DisplayNameEn)
		: FString::Printf(TEXT("统帅 %d"), SelectedCommanderId);
	const FString SelectedTitleString = bHasSelectedHero
		? (!SelectedHero.DisplayTitleZhCn.IsEmpty()
			? SelectedHero.DisplayTitleZhCn
			: SelectedHero.DisplayTitleEn)
		: FString();
	const FString SelectedDescriptionString = bHasSelectedHero
		? (!SelectedHero.DisplayDescriptionZhCn.IsEmpty()
			? SelectedHero.DisplayDescriptionZhCn
			: SelectedHero.DisplayDescriptionEn)
		: FString();

	FText CommanderSortLabel;
	switch (SelectedCommanderSortType)
	{
	case 1:
		CommanderSortLabel = NSLOCTEXT(
			"RokCommander",
			"StarSort",
			"星级 ▼");
		break;
	case 2:
		CommanderSortLabel = NSLOCTEXT(
			"RokCommander",
			"LevelSort",
			"等级 ▼");
		break;
	case 3:
		CommanderSortLabel = NSLOCTEXT(
			"RokCommander",
			"PowerSort",
			"战力 ▼");
		break;
	default:
		CommanderSortLabel = NSLOCTEXT(
			"RokCommander",
			"QualitySort",
			"品质 ▼");
		break;
	}

	TArray<FText> CommanderRoleLabels;
	VisibleCommanderTalentIds.Reset();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const int32 TalentId = bHasSelectedHero && SelectedHero.TalentIds.IsValidIndex(Index)
			? SelectedHero.TalentIds[Index] : INDEX_NONE;
		VisibleCommanderTalentIds.Add(TalentId);
		FRokUnityCatalogHeroTalent Talent;
		const bool bHasTalent = GameplayCatalog && TalentId > 0
			&& GameplayCatalog->GetHeroTalent(TalentId, Talent);
		const FString TalentName = bHasTalent
			? (!Talent.DisplayNameZhCn.IsEmpty() ? Talent.DisplayNameZhCn : Talent.DisplayNameEn)
			: FString();
		CommanderRoleLabels.Add(TalentName.IsEmpty()
			? NSLOCTEXT("RokCommander", "TalentPositionUnavailable", "—")
			: FText::FromString(TalentName));
	}

	VisibleCommanderSkillIds.Reset();
	if (bHasSelectedHero)
	{
		for (const int32 SkillId : SelectedHero.SkillIds)
		{
			VisibleCommanderSkillIds.Add(SkillId);
			if (VisibleCommanderSkillIds.Num() == 5)
			{
				break;
			}
		}
	}
	else if (!GameplayCatalog && SelectedCommanderId == 2007)
	{
		for (int32 SkillId = 200701; SkillId <= 200705; ++SkillId)
		{
			VisibleCommanderSkillIds.Add(SkillId);
		}
	}
	SelectedCommanderSkillIndex = VisibleCommanderSkillIds.IsEmpty()
		? 0
		: FMath::Clamp(
			SelectedCommanderSkillIndex,
			0,
			VisibleCommanderSkillIds.Num() - 1);
	FRokUnityCatalogHeroSkill SelectedCommanderSkill;
	const bool bHasSelectedCommanderSkill = GameplayCatalog
		&& VisibleCommanderSkillIds.IsValidIndex(
			SelectedCommanderSkillIndex)
		&& GameplayCatalog->GetHeroSkill(
			VisibleCommanderSkillIds[SelectedCommanderSkillIndex],
			SelectedCommanderSkill);
	const auto ResolveCommanderHeadRect =
		[GameplayCatalog](int32 HeroId, FIntRect& OutRect)
		{
			FRokUnityCatalogHero Hero;
			if (GameplayCatalog
				&& GameplayCatalog->GetHero(HeroId, Hero)
				&& Hero.IconAtlas.Equals(
					TEXT("hero_head"),
					ESearchCase::IgnoreCase)
				&& Hero.IconWidth > 0
				&& Hero.IconHeight > 0
				&& Hero.IconAtlasWidth > 0
				&& Hero.IconAtlasHeight > 0)
			{
				OutRect = FIntRect(
					Hero.IconX,
					Hero.IconY,
					Hero.IconX + Hero.IconWidth,
					Hero.IconY + Hero.IconHeight);
				return true;
			}
			return ResolveFallbackCommanderHeadRect(HeroId, OutRect);
		};

	// Unity's UI_IF_Captain keeps img_bg inactive and presents the commander in
	// UI_3D_Scene/HeroShow. Keep this stage neutral until that 3D presentation
	// path is migrated instead of stretching the unrelated desert atlas.
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("CommanderHeroShowStage"),
			FLinearColor(0.075f, 0.105f, 0.105f, 1.0f)),
		CapturePoint(0.0f, 0.0f),
		FVector2D(1440.0f, 900.0f),
		0);
	// CaptainMediator keeps the loaded skeleton while updating same-hero details.
	// Reattach that widget after rebuilding the panel instead of restarting idle.
	if (CommanderAnimatedPortraitHeroId != SelectedCommanderId)
	{
		CommanderAnimatedPortraitHeroId = SelectedCommanderId;
		CommanderAnimatedPortrait = WidgetTree->ConstructWidget<URokCommanderMeshWidget>(
			URokCommanderMeshWidget::StaticClass(), TEXT("CommanderAnimatedPortrait"));
		if (!CommanderAnimatedPortrait->LoadHero(SelectedCommanderId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Commander %d has no supported generated animation; showing diagnostic portrait only."), SelectedCommanderId);
			CommanderAnimatedPortrait = nullptr;
		}
	}
	if (CommanderAnimatedPortrait)
	{
		PlaceOnCanvas(ModeCanvas,
			MakeActionButton(TEXT("CommanderPortraitAction"), 499, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(650.0f, 740.0f)),
			CapturePoint(300.0f, 105.0f), FVector2D(650.0f, 740.0f), 3);
		PlaceOnCanvas(
			ModeCanvas,
			CommanderAnimatedPortrait,
			CapturePoint(0.0f, 0.0f),
			FVector2D(1440.0f, 900.0f),
			2);
	}
	else if (SelectedCommanderId == 2007)
	{
		PlaceOnCanvas(ModeCanvas,
			MakeAtlasImage(TEXT("CommanderHero2007"), CommanderPortraitPath, CommanderPortraitRect, CommanderPortraitSize, FVector2D(650.0f, 740.0f)),
			CapturePoint(300.0f, 105.0f), FVector2D(650.0f, 740.0f), 2);
	}
	else
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				TEXT("CommanderHeroShowPortraitShade"),
				FLinearColor(0.025f, 0.035f, 0.035f, 0.66f)),
			CapturePoint(438.0f, 210.0f),
			FVector2D(374.0f, 438.0f),
			1);
		FIntRect SelectedHeadRect;
		if (ResolveCommanderHeadRect(
			SelectedCommanderId,
			SelectedHeadRect))
		{
			const FVector2D SelectedHeadBounds(330.0f, 374.0f);
			const float SelectedHeadScale = FMath::Min(
				SelectedHeadBounds.X / static_cast<float>(SelectedHeadRect.Width()),
				SelectedHeadBounds.Y / static_cast<float>(SelectedHeadRect.Height()));
			const FVector2D SelectedHeadSize(
				static_cast<float>(SelectedHeadRect.Width()) * SelectedHeadScale,
				static_cast<float>(SelectedHeadRect.Height()) * SelectedHeadScale);
			const FVector2D SelectedHeadPosition(
				460.0f + (SelectedHeadBounds.X - SelectedHeadSize.X) * 0.5f,
				232.0f + (SelectedHeadBounds.Y - SelectedHeadSize.Y) * 0.5f);
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					TEXT("CommanderSelectedHeadPreview"),
					CommanderHeadAtlasPath,
					SelectedHeadRect,
					CommanderHeadAtlasSize,
					SelectedHeadSize),
				CapturePoint(
					SelectedHeadPosition.X,
					SelectedHeadPosition.Y),
				SelectedHeadSize,
				2);
		}
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("CommanderRosterShade"), FLinearColor(0.035f, 0.035f, 0.035f, 0.70f)),
		CapturePoint(13.0f, 119.0f),
		FVector2D(304.0f, 758.0f),
		3);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("CommanderRosterHeader"), FLinearColor(0.025f, 0.025f, 0.025f, 0.88f)),
		CapturePoint(13.0f, 119.0f),
		FVector2D(304.0f, 52.0f),
		4);
	UTextBlock* OwnedCount = MakeScreenText(TEXT("CommanderOwnedCount"), 16, FLinearColor(0.96f, 0.96f, 0.94f, 1.0f), false);
	OwnedCount->SetText(FText::Format(
		NSLOCTEXT(
			"RokCommander",
			"OwnedCount",
			"已拥有 {0}"),
		FText::AsNumber(RuntimeHeroes.Num())));
	PlaceOnCanvas(ModeCanvas, OwnedCount, CapturePoint(26.0f, 132.0f), FVector2D(150.0f, 30.0f), 7);
	UTextBlock* Sort = MakeScreenText(TEXT("CommanderSortLabel"), 17, FLinearColor(0.92f, 0.92f, 0.9f, 1.0f), false, ETextJustify::Right);
	Sort->SetText(CommanderSortLabel);
	PlaceOnCanvas(ModeCanvas, Sort, CapturePoint(182.0f, 132.0f), FVector2D(113.0f, 30.0f), 7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("CommanderSortAction"), 470, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(130.0f, 44.0f)),
		CapturePoint(170.0f, 122.0f),
		FVector2D(130.0f, 44.0f),
		8);

	UScrollBox* CommanderRosterScroll =
		WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(),
			TEXT("CommanderRosterScroll"));
	CommanderRosterScroll->SetOrientation(Orient_Vertical);
	CommanderRosterScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	CommanderRosterScroll->SetAllowOverscroll(false);
	CommanderRosterScroll->SetAnimateWheelScrolling(true);
	CommanderRosterScroll->SetWheelScrollMultiplier(12.0f);
	PlaceOnCanvas(
		ModeCanvas,
		CommanderRosterScroll,
		CapturePoint(13.0f, 171.0f),
		FVector2D(304.0f, 684.0f),
		5);

	const int32 CommanderRowCount =
		FMath::DivideAndRoundUp(VisibleCommanderIds.Num(), 2);
	for (int32 RowIndex = 0; RowIndex < CommanderRowCount; ++RowIndex)
	{
		UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("CommanderRosterRow_%d"), RowIndex));
		for (int32 ColumnIndex = 0; ColumnIndex < 2; ++ColumnIndex)
		{
			const int32 Index = RowIndex * 2 + ColumnIndex;
			if (!VisibleCommanderIds.IsValidIndex(Index))
			{
				break;
			}
			const int32 HeroId = VisibleCommanderIds[Index];
			FRokUnityCatalogHero RosterHero;
			const bool bHasRosterHero = GameplayCatalog
				&& GameplayCatalog->GetHero(HeroId, RosterHero);
			const FString RosterHeroName = bHasRosterHero
				? (!RosterHero.DisplayNameZhCn.IsEmpty()
					? RosterHero.DisplayNameZhCn
					: RosterHero.DisplayNameEn)
				: FString::Printf(TEXT("统帅 %d"), HeroId);
			const FRokUnityRuntimeHeroState* RosterRuntimeHero =
				FindRuntimeHero(HeroId);
			const FText RosterStateText = RosterRuntimeHero
				? FText::Format(
					NSLOCTEXT("RokCommander", "RosterOwnedState", "Lv.{0}  ★{1}"),
					FText::AsNumber(RosterRuntimeHero->Level),
					FText::AsNumber(RosterRuntimeHero->Star))
				: FText::Format(
					NSLOCTEXT("RokCommander", "RosterShardState", "需要 {0}"),
					FText::AsNumber(bHasRosterHero
						? FMath::Max(1, RosterHero.GetItemNum)
						: 10));
			FIntRect HeadRect;
			if (!ResolveCommanderHeadRect(HeroId, HeadRect))
			{
				HeadRect = FIntRect();
			}
			PlaceOnCanvas(
				Row,
				MakeCommanderHead(
					Index,
					HeadRect,
					RosterRuntimeHero != nullptr,
					HeroId == SelectedCommanderId,
					FText::FromString(RosterHeroName),
					RosterStateText,
					bHasRosterHero ? RosterHero.Rare : 0),
				FVector2D(11.0f + ColumnIndex * 140.0f, 4.0f),
				FVector2D(128.0f, 142.0f),
				1);
		}
		CommanderRosterScroll->AddChild(WrapFixedSize(
			Row,
			*FString::Printf(TEXT("CommanderRosterRowSize_%d"), RowIndex),
			FVector2D(304.0f, 150.0f)));
	}

	if (bCommanderSortMenuOpen)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				TEXT("CommanderSortMenu"),
				FLinearColor(0.035f, 0.035f, 0.035f, 0.98f)),
			CapturePoint(156.0f, 168.0f),
			FVector2D(144.0f, 180.0f),
			30);
		static const FText SortLabels[] = {
			NSLOCTEXT("RokCommander", "SortQuality", "品质"),
			NSLOCTEXT("RokCommander", "SortStar", "星级"),
			NSLOCTEXT("RokCommander", "SortLevel", "等级"),
			NSLOCTEXT("RokCommander", "SortPower", "战力"),
		};
		for (int32 SortIndex = 0; SortIndex < UE_ARRAY_COUNT(SortLabels); ++SortIndex)
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeActionButton(
					*FString::Printf(TEXT("CommanderSortOption_%d"), SortIndex),
					472 + SortIndex,
					nullptr,
					FIntRect(),
					FIntPoint::ZeroValue,
					FVector2D(136.0f, 42.0f),
					SortLabels[SortIndex],
					18,
					SortIndex == SelectedCommanderSortType
						? FLinearColor(1.0f, 0.78f, 0.24f, 1.0f)
						: FLinearColor(0.94f, 0.94f, 0.92f, 1.0f)),
				CapturePoint(160.0f, 172.0f + SortIndex * 43.0f),
				FVector2D(136.0f, 42.0f),
				31);
		}
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("CommanderBack"), 1, CommonAtlasPath, CommanderBackRect, CommonAtlasSize, FVector2D(66.0f)),
		CapturePoint(18.0f, 52.0f),
		FVector2D(66.0f),
		12);

	// Unity UI_Item_CaptainData owns this group, separately from roster and hero.
	const int32 CommanderDataFirstChild = ModeCanvas->GetChildrenCount();
	UTextBlock* Appellation = MakeScreenText(TEXT("CommanderAppellation"), 20, FLinearColor(0.88f, 0.88f, 0.84f, 1.0f), false);
	Appellation->SetText(FText::FromString(
		SelectedTitleString.IsEmpty()
			? FString(TEXT("◉  统帅"))
			: FString::Printf(TEXT("◉  %s"), *SelectedTitleString)));
	PlaceOnCanvas(ModeCanvas, Appellation, CapturePoint(975.0f, 145.0f), FVector2D(340.0f, 35.0f), 7);
	UTextBlock* Name = MakeScreenText(TEXT("CommanderName"), 35, FLinearColor(0.96f, 0.94f, 0.86f, 1.0f), true);
	Name->SetText(FText::FromString(SelectedNameString));
	PlaceOnCanvas(ModeCanvas, Name, CapturePoint(975.0f, 180.0f), FVector2D(380.0f, 54.0f), 7);

	const FLinearColor RoleColors[] = {
		FLinearColor(0.55f, 0.17f, 0.13f, 0.78f),
		FLinearColor(0.18f, 0.50f, 0.31f, 0.78f),
		FLinearColor(0.16f, 0.31f, 0.55f, 0.78f),
	};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float X = 975.0f + Index * 118.0f;
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(*FString::Printf(TEXT("CommanderRoleBg_%d"), Index), RoleColors[Index]), CapturePoint(X, 236.0f), FVector2D(108.0f, 31.0f), 6);
		PlaceOnCanvas(ModeCanvas,
			MakeActionButton(*FString::Printf(TEXT("CommanderTalentLabel_%d"), Index),
				490 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue,
				FVector2D(108.0f, 29.0f), CommanderRoleLabels[Index], 18,
				FLinearColor::White),
			CapturePoint(X, 237.0f), FVector2D(108.0f, 29.0f), 8);
	}

	// Config-only preview of the Unity TalentTop -> TalentSpecialPop click/close path.
	// No point, current-level, account-ownership or 451 talent-tree state is inferred.
	if (VisibleCommanderTalentIds.IsValidIndex(SelectedCommanderTalentIndex)
		&& SelectedRuntimeHero && GameplayCatalog)
	{
		FRokUnityCatalogHeroTalent DetailTalent;
		const int32 DetailTalentId = VisibleCommanderTalentIds[SelectedCommanderTalentIndex];
		if (GameplayCatalog->GetHeroTalent(DetailTalentId, DetailTalent))
		{
			const FString DetailName = !DetailTalent.DisplayNameZhCn.IsEmpty()
				? DetailTalent.DisplayNameZhCn : DetailTalent.DisplayNameEn;
			const FString DetailTips = !DetailTalent.DisplayTipsZhCn.IsEmpty()
				? DetailTalent.DisplayTipsZhCn : DetailTalent.DisplayTipsEn;
			if (!DetailName.IsEmpty() && !DetailTips.IsEmpty())
			{
				PlaceOnCanvas(ModeCanvas,
					MakeAtlasBorder(TEXT("CommanderTalentDetailFrame"), CommonAtlasPath,
						SpriteRect(774, 276, 72, 72), CommonAtlasSize, FVector2D(430.0f, 260.0f),
						FMargin(21.0f / 72, 23.0f / 72, 21.0f / 72, 24.0f / 72)),
					CapturePoint(965.0f, 285.0f), FVector2D(430.0f, 260.0f), 15);
				UTextBlock* DetailTitle = MakeScreenText(TEXT("CommanderTalentDetailTitle"),
					22, FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f), true);
				DetailTitle->SetText(FText::FromString(DetailName));
				DetailTitle->SetShadowColorAndOpacity(FLinearColor::Transparent);
				PlaceOnCanvas(ModeCanvas, DetailTitle, CapturePoint(986.0f, 309.0f),
					FVector2D(335.0f, 36.0f), 16);
				UTextBlock* DetailDescription = MakeScreenText(TEXT("CommanderTalentDetailTips"),
					18, FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f), true);
				DetailDescription->SetText(FText::FromString(DetailTips));
				DetailDescription->SetShadowColorAndOpacity(FLinearColor::Transparent);
				DetailDescription->SetAutoWrapText(true);
				DetailDescription->SetWrapTextAt(375.0f);
				PlaceOnCanvas(ModeCanvas, DetailDescription, CapturePoint(986.0f, 357.0f),
					FVector2D(375.0f, 155.0f), 16);
				PlaceOnCanvas(ModeCanvas,
					MakeActionButton(TEXT("CommanderTalentDetailClose"), 493,
						CommonAtlasPath, CloseRect, CommonAtlasSize, FVector2D(44.0f)),
					CapturePoint(1340.0f, 300.0f), FVector2D(44.0f), 17);
			}
		}
	}

	if (bSelectedOwned)
	{
		for (int32 Index = 0; Index < 6; ++Index)
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeAtlasImage(
					*FString::Printf(TEXT("CommanderStar_%d"), Index),
					CommonAtlasPath,
					Index < SelectedStar && Index < SelectedMaxStar
						? CommanderStarOnRect
						: CommanderStarOffRect,
					CommonAtlasSize,
					FVector2D(48.0f)),
				CapturePoint(976.0f + Index * 57.0f, 279.0f),
				FVector2D(48.0f),
				7);
		}
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			bSelectedOwned
				? TEXT("CommanderOwnedPanel")
				: TEXT("CommanderUnownedPanel"),
			FLinearColor(0.96f, 0.94f, 0.89f, 0.22f)),
		CapturePoint(965.0f, 345.0f),
		FVector2D(430.0f, bSelectedOwned ? 168.0f : 238.0f),
		4);
	UTextBlock* Level = MakeScreenText(TEXT("CommanderLevel"), 20, FLinearColor(0.96f, 0.95f, 0.91f, 1.0f));
	Level->SetText(bSelectedOwned
		? FText::Format(
			NSLOCTEXT("RokCommander", "OwnedLevel", "等级 {0}"),
			FText::AsNumber(SelectedLevel))
		: NSLOCTEXT("RokCommander", "NotRecruited", "尚未招募"));
	PlaceOnCanvas(ModeCanvas, Level, CapturePoint(975.0f, 361.0f), FVector2D(170.0f, 32.0f), 7);
	if (!bSelectedOwned)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeSolidBorder(
				TEXT("CommanderSummonPanel"),
				FLinearColor(0.08f, 0.075f, 0.055f, 0.92f)),
			CapturePoint(975.0f, 397.0f),
			FVector2D(405.0f, 102.0f),
			6);
		UTextBlock* SummonRequirement = MakeScreenText(
			TEXT("CommanderSummonRequirement"),
			18,
			FLinearColor(0.95f, 0.91f, 0.76f, 1.0f),
			false,
			ETextJustify::Center);
		SummonRequirement->SetText(FText::Format(
			NSLOCTEXT("RokCommander", "SummonRequirement", "召唤需要 {0} 个统帅雕像"),
			FText::AsNumber(bHasSelectedHero
				? FMath::Max(1, SelectedHero.GetItemNum)
				: 10)));
		PlaceOnCanvas(ModeCanvas, SummonRequirement, CapturePoint(985.0f, 406.0f), FVector2D(385.0f, 30.0f), 7);
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				TEXT("CommanderSummonAction"),
				452,
				CommonAtlasPath,
				MediumGoldButtonRect,
				CommonAtlasSize,
				FVector2D(170.0f, 54.0f),
				NSLOCTEXT("RokCommander", "Summon", "招募"),
				22,
				FLinearColor(0.17f, 0.10f, 0.02f, 1.0f)),
			CapturePoint(1092.0f, 440.0f),
			FVector2D(170.0f, 54.0f),
			8);
	}
	UTextBlock* Capacity = MakeScreenText(TEXT("CommanderCapacity"), 20, FLinearColor(0.96f, 0.95f, 0.91f, 1.0f));
	Capacity->SetText(FText::Format(
		NSLOCTEXT(
			"RokCommander",
			"CommanderIdentity",
			"稀有度 {0}  ·  文明 {1}"),
		FText::AsNumber(bHasSelectedHero ? SelectedHero.Rare : 0),
		FText::AsNumber(bHasSelectedHero ? SelectedHero.Civilization : 0)));
	PlaceOnCanvas(ModeCanvas, Capacity, CapturePoint(975.0f, bSelectedOwned ? 464.0f : 510.0f), FVector2D(400.0f, 34.0f), 7);
	UTextBlock* CommanderDescription = MakeScreenText(
		TEXT("CommanderDescription"),
		15,
		FLinearColor(0.88f, 0.87f, 0.84f, 1.0f));
	FString CommanderDescriptionPreview = SelectedDescriptionString.IsEmpty()
		? TEXT("暂无统帅介绍。")
		: SelectedDescriptionString;
	if (CommanderDescriptionPreview.Len() > 58)
	{
		CommanderDescriptionPreview = CommanderDescriptionPreview.Left(58)
			+ TEXT("…");
	}
	CommanderDescription->SetText(FText::FromString(
		CommanderDescriptionPreview));
	CommanderDescription->SetAutoWrapText(true);
	CommanderDescription->SetWrapTextAt(405.0f);
	PlaceOnCanvas(
		ModeCanvas,
		CommanderDescription,
		CapturePoint(975.0f, bSelectedOwned ? 495.0f : 541.0f),
		FVector2D(405.0f, 48.0f),
		7);

	if (bSelectedOwned)
	{
	UTextBlock* SkillTitle = MakeScreenText(TEXT("CommanderSkillTitle"), 20, FLinearColor(0.96f, 0.95f, 0.91f, 1.0f));
	SkillTitle->SetText(NSLOCTEXT("RokCommander", "Skills", "技能"));
	PlaceOnCanvas(ModeCanvas, SkillTitle, CapturePoint(975.0f, 555.0f), FVector2D(160.0f, 34.0f), 7);
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("CommanderSkillShade"), FLinearColor(0.08f, 0.06f, 0.045f, 0.55f)), CapturePoint(972.0f, 584.0f), FVector2D(415.0f, 102.0f), 5);
	for (int32 Index = 0; Index < VisibleCommanderSkillIds.Num(); ++Index)
	{
		const float X = 978.0f + Index * 81.0f;
		FRokUnityCatalogHeroSkill Skill;
		const bool bHasSkill = GameplayCatalog
			&& GameplayCatalog->GetHeroSkill(
				VisibleCommanderSkillIds[Index],
				Skill);
		FIntRect SkillRect;
		bool bHasSkillIcon = bHasSkill
			&& (Skill.IconAtlas.Equals(
					TEXT("hero_skill"),
					ESearchCase::IgnoreCase)
				|| Skill.IconAtlas.Equals(
					TEXT("hero_skill2"),
					ESearchCase::IgnoreCase))
			&& Skill.IconWidth > 0
			&& Skill.IconHeight > 0
			&& Skill.IconAtlasWidth > 0
			&& Skill.IconAtlasHeight > 0;
		if (bHasSkillIcon)
		{
			SkillRect = FIntRect(
				Skill.IconX,
				Skill.IconY,
				Skill.IconX + Skill.IconWidth,
				Skill.IconY + Skill.IconHeight);
		}
		else if (SelectedCommanderId == 2007
			&& Index < UE_ARRAY_COUNT(CommanderSkillRects))
		{
			SkillRect = CommanderSkillRects[Index];
			bHasSkillIcon = true;
		}
		if (bHasSkillIcon)
		{
			const TCHAR* SkillTexturePath = Skill.IconAtlas.Equals(
				TEXT("hero_skill"),
				ESearchCase::IgnoreCase)
				? CommanderSkillAtlasPath
				: CommanderSkill2AtlasPath;
			UImage* SkillImage = MakeAtlasImage(
				*FString::Printf(TEXT("CommanderSkillIcon_%d"), Index),
				SkillTexturePath,
				SkillRect,
				CommanderSkillAtlasSize,
				FVector2D(74.0f));
			if (Index != SelectedCommanderSkillIndex)
			{
				SkillImage->SetColorAndOpacity(
					FLinearColor(0.55f, 0.55f, 0.55f, 1.0f));
			}
			PlaceOnCanvas(
				ModeCanvas,
				SkillImage,
				CapturePoint(X, 594.0f),
				FVector2D(74.0f),
				7);
		}
		else
		{
			PlaceOnCanvas(
				ModeCanvas,
				MakeSolidBorder(
					*FString::Printf(TEXT("CommanderSkillPending_%d"), Index),
					Index == SelectedCommanderSkillIndex
						? FLinearColor(0.46f, 0.34f, 0.10f, 0.92f)
						: FLinearColor(0.10f, 0.10f, 0.105f, 0.92f)),
				CapturePoint(X, 594.0f),
				FVector2D(74.0f),
				7);
		}
		UTextBlock* SkillIndexLabel = MakeScreenText(
			*FString::Printf(TEXT("CommanderSkillIndex_%d"), Index),
			13,
			FLinearColor::White,
			true,
			ETextJustify::Center);
		SkillIndexLabel->SetText(Index == SelectedCommanderSkillIndex
			? NSLOCTEXT(
				"RokCommander",
				"SelectedSkillIndicator",
				"已选")
			: FText::GetEmpty());
		PlaceOnCanvas(
			ModeCanvas,
			SkillIndexLabel,
			CapturePoint(X + 2.0f, 645.0f),
			FVector2D(70.0f, 20.0f),
			8);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("CommanderSkillAction_%d"), Index), 460 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(74.0f)), CapturePoint(X, 594.0f), FVector2D(74.0f), 9);
	}
	UTextBlock* SelectedSkillSummary = MakeScreenText(
		TEXT("CommanderSelectedSkillSummary"),
		15,
		FLinearColor(0.96f, 0.90f, 0.72f, 1.0f));
	if (bHasSelectedCommanderSkill)
	{
		const FString SkillName = !SelectedCommanderSkill.DisplayNameZhCn.IsEmpty()
			? SelectedCommanderSkill.DisplayNameZhCn
			: SelectedCommanderSkill.DisplayNameEn;
		const FString SkillType = !SelectedCommanderSkill.DisplayTypeZhCn.IsEmpty()
			? SelectedCommanderSkill.DisplayTypeZhCn
			: SelectedCommanderSkill.DisplayTypeEn;
		SelectedSkillSummary->SetText(FText::FromString(FString::Printf(
			TEXT("%s  ·  %s  ·  %s"),
			SkillName.IsEmpty() ? TEXT("技能") : *SkillName,
			SkillType.IsEmpty() ? TEXT("技能") : *SkillType,
			SelectedCommanderSkill.DisplayOpenZhCn.IsEmpty()
				? TEXT("未解锁")
				: *SelectedCommanderSkill.DisplayOpenZhCn)));
	}
	else
	{
		SelectedSkillSummary->SetText(NSLOCTEXT(
			"RokCommander",
			"NoSkillInformation",
			"暂无技能信息。"));
	}
	PlaceOnCanvas(
		ModeCanvas,
		SelectedSkillSummary,
		CapturePoint(978.0f, 688.0f),
		FVector2D(405.0f, 28.0f),
		8);

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("CommanderSkillUpgrade"), 450, CommonAtlasPath, LargeBlueButtonRect, CommonAtlasSize, FVector2D(195.0f, 66.0f), NSLOCTEXT("RokCommander", "SkillUpgrade", "技能升级"), 25, FLinearColor(0.03f, 0.14f, 0.25f, 1.0f)),
		CapturePoint(978.0f, 724.0f),
		FVector2D(195.0f, 66.0f),
		8);
	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("CommanderTalent"), 451, CommonAtlasPath, LargeBlueButtonRect, CommonAtlasSize, FVector2D(195.0f, 66.0f), NSLOCTEXT("RokCommander", "Talent", "天赋"), 25, FLinearColor(0.03f, 0.14f, 0.25f, 1.0f)),
		CapturePoint(1192.0f, 724.0f),
		FVector2D(195.0f, 66.0f),
		8);
	}
	// Source UI_Item_CaptainSkill_SubView.OnSkillClick supplies configuration tips.
	// Keep the five skill actions below this overlay clickable for real switching.
	if (VisibleCommanderSkillIds.IsValidIndex(SelectedCommanderSkillDetailIndex)
		&& SelectedRuntimeHero && GameplayCatalog)
	{
		FRokUnityCatalogHeroSkill DetailSkill;
		FRokCommanderSkillReadonlyDetail Detail;
		if (GameplayCatalog->GetHeroSkill(
			VisibleCommanderSkillIds[SelectedCommanderSkillDetailIndex], DetailSkill)
			&& RokCommanderSkillReadonlyDetail::Build(DetailSkill, Detail))
		{
			PlaceOnCanvas(ModeCanvas,
				MakeAtlasBorder(TEXT("CommanderSkillDetailFrame"), CommonAtlasPath,
					SpriteRect(774, 276, 72, 72), CommonAtlasSize, FVector2D(478.0f, 405.0f),
					FMargin(21.0f / 72, 23.0f / 72, 21.0f / 72, 24.0f / 72)),
				CapturePoint(922.0f, 170.0f), FVector2D(478.0f, 405.0f), 15);
			UTextBlock* DetailTitle = MakeScreenText(TEXT("CommanderSkillDetailTitle"),
				22, FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f));
			DetailTitle->SetText(FText::FromString(Detail.Title));
			DetailTitle->SetShadowColorAndOpacity(FLinearColor::Transparent);
			PlaceOnCanvas(ModeCanvas, DetailTitle, CapturePoint(943.0f, 187.0f),
				FVector2D(385.0f, 35.0f), 16);
			UTextBlock* DetailBody = MakeScreenText(TEXT("CommanderSkillDetailBody"),
				16, FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f));
			DetailBody->SetText(FText::FromString(Detail.Body));
			DetailBody->SetShadowColorAndOpacity(FLinearColor::Transparent);
			DetailBody->SetAutoWrapText(true);
			DetailBody->SetWrapTextAt(418.0f);
			UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(
				UScrollBox::StaticClass(), TEXT("CommanderSkillDetailScroll"));
			DetailScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
			DetailScroll->AddChild(DetailBody);
			PlaceOnCanvas(ModeCanvas, DetailScroll, CapturePoint(943.0f, 232.0f),
				FVector2D(432.0f, 322.0f), 16);
			PlaceOnCanvas(ModeCanvas,
				MakeActionButton(TEXT("CommanderSkillDetailClose"), 465,
					CommonAtlasPath, CloseRect, CommonAtlasSize, FVector2D(44.0f)),
				CapturePoint(1340.0f, 184.0f), FVector2D(44.0f), 17);
		}
	}
	for (int32 Index = CommanderDataFirstChild; Index < ModeCanvas->GetChildrenCount(); ++Index)
	{
		CommanderDataAnimationWidgets.Add(ModeCanvas->GetChildAt(Index));
	}
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("CommanderPowerShade"), FLinearColor(0.09f, 0.06f, 0.04f, 0.56f)), CapturePoint(527.0f, 778.0f), FVector2D(390.0f, 36.0f), 5);
	UTextBlock* Power = MakeScreenText(TEXT("CommanderPower"), 22, FLinearColor(0.94f, 0.92f, 0.88f, 1.0f), false, ETextJustify::Center);
	Power->SetText(FText::Format(
		bSelectedOwned
			? NSLOCTEXT(
				"RokCommander",
				"OwnedPower",
				"战力 {0}")
			: NSLOCTEXT(
				"RokCommander",
				"BasePower",
				"基础战力 {0}"),
		FText::AsNumber(SelectedPower)));
	PlaceOnCanvas(ModeCanvas, Power, CapturePoint(527.0f, 779.0f), FVector2D(390.0f, 34.0f), 7);
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Rok commander screen: hero=%d owned=%d sort=%d roster=%d stage=hero_show_boundary."),
		SelectedCommanderId,
		bSelectedOwned ? 1 : 0,
		SelectedCommanderSortType,
		VisibleCommanderIds.Num());
	// Keep existing canvas slots/z-order; only the source list subtree moves.
	// Animate direct children only, so scrolling rows don't get transformed twice.
	for (UWidget* Child : ModeCanvas->GetAllChildren())
	{
		const FString ChildName = Child->GetName();
		if (ChildName.StartsWith(TEXT("CommanderRoster")) || ChildName.StartsWith(TEXT("CommanderSort"))
			|| ChildName == TEXT("CommanderOwnedCount"))
			CommanderRosterAnimationWidgets.Add(Child);
	}
	AdvanceCommanderRosterAnimation(0.0f);
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

void URokUnityFeatureScreenWidget::BuildItems()
{
	BuildSplitWindowFrame(
		CapturePoint(146.0f, 73.0f),
		FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokItems", "Title", "道具"),
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("ItemsContentShade"),
			FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 138.1f),
		FVector2D(1088.0f, 683.8f),
		2);

	static const FText CategoryLabels[] = {
		NSLOCTEXT("RokItems", "Resources", "资源"),
		NSLOCTEXT("RokItems", "Speedups", "加速"),
		NSLOCTEXT("RokItems", "Boosts", "增益"),
		NSLOCTEXT("RokItems", "Equipment", "装备"),
		NSLOCTEXT("RokItems", "Other", "其他"),
	};
	for (int32 Category = 0; Category < UE_ARRAY_COUNT(CategoryLabels); ++Category)
	{
		const bool bSelected = Category == SelectedItemsCategory;
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(
				*FString::Printf(TEXT("ItemsCategory_%d"), Category),
				520 + Category,
				CommonAtlasPath,
				bSelected ? PageSelectedRect : PageNormalRect,
				CommonAtlasSize,
				FVector2D(214.0f, 45.0f),
				CategoryLabels[Category],
				24,
				bSelected ? FLinearColor(0.96f, 0.97f, 0.95f, 1.0f) : FLinearColor(0.72f, 0.72f, 0.70f, 1.0f)),
			CapturePoint(184.0f + Category * 214.4f, 154.0f),
			FVector2D(214.0f, 45.0f),
			6);
	}

	URokUnityEconomyCatalogSubsystem* Catalog =
		GetEconomyCatalogSubsystem();
	if (Catalog && !Catalog->IsCatalogLoaded())
	{
		Catalog->LoadCatalog();
	}

	struct FItemsPreviewStack
	{
		int64 ItemIndex = 0;
		int32 ItemId = 0;
		int64 Overlay = 0;
		FRokUnityEconomyItem Item;
	};
	struct FItemsPreviewStackSeed
	{
		int64 ItemIndex;
		int32 ItemId;
		int64 Overlay;
	};
	static const FItemsPreviewStackSeed PreviewStacks[] = {
		{900001, 101050010, 3},
		{900002, 101050020, 12},
		{900003, 101040010, 5},
		{900004, 101040020, 8},
		{900005, 101030010, 4},
		{900006, 101020010, 2},
		{900007, 101050001, 10000},
		// Unity BagProxy.Items ignores stacks whose overlay is not positive.
		{900008, 101040001, 0},
	};

	TArray<FItemsPreviewStack> VisibleStacks;
	if (bItemsTestFixtureState && Catalog)
	{
		const int32 SelectedItemType = SelectedItemsCategory + 1;
		for (const FItemsPreviewStackSeed& Seed : PreviewStacks)
		{
			FRokUnityEconomyItem Item;
			if (Seed.Overlay <= 0
				|| !Catalog->FindItemById(Seed.ItemId, Item)
				|| Item.Type != SelectedItemType)
			{
				continue;
			}

			FItemsPreviewStack& Stack = VisibleStacks.AddDefaulted_GetRef();
			Stack.ItemIndex = Seed.ItemIndex;
			Stack.ItemId = Seed.ItemId;
			Stack.Overlay = Seed.Overlay;
			Stack.Item = MoveTemp(Item);
		}
		VisibleStacks.Sort(
			[](const FItemsPreviewStack& Left,
				const FItemsPreviewStack& Right)
			{
				return Left.Item.Rank == Right.Item.Rank
					? Left.ItemId < Right.ItemId
					: Left.Item.Rank < Right.Item.Rank;
			});
	}

	VisibleItemsStacks.Reset();
	for (const FItemsPreviewStack& Stack : VisibleStacks)
	{
		FItemsInventoryStackState& State =
			VisibleItemsStacks.AddDefaulted_GetRef();
		State.ItemIndex = Stack.ItemIndex;
		State.ItemId = Stack.ItemId;
		State.Overlay = Stack.Overlay;
	}
	SelectedItemsIndex = FMath::Clamp(
		SelectedItemsIndex,
		0,
		FMath::Max(0, VisibleItemsStacks.Num() - 1));
	SelectedItemsUseCount = FMath::Clamp(
		SelectedItemsUseCount,
		1,
		VisibleItemsStacks.IsValidIndex(SelectedItemsIndex)
			? static_cast<int32>(FMath::Clamp<int64>(
				VisibleItemsStacks[SelectedItemsIndex].Overlay,
				1,
				9999))
			: 1);

	if (VisibleStacks.IsEmpty())
	{
		UTextBlock* EmptyState = MakeScreenText(
			TEXT("ItemsInventoryEmptyState"),
			26,
			FLinearColor(0.8509804f, 0.8313726f, 0.7294118f, 1.0f),
			false,
			ETextJustify::Center);
		EmptyState->SetText(NSLOCTEXT(
			"RokItems",
			"InventoryEmpty",
			"您的道具库里真是空空如也，快获取一些道具来充实它吧！"));
		EmptyState->SetAutoWrapText(true);
		PlaceOnCanvas(
			ModeCanvas,
			EmptyState,
			CapturePoint(198.9f, 394.0f),
			FVector2D(1042.3f, 220.7f),
			6);
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Rok items screen: inventory=empty fixture=%d category=%d rows=0."),
			bItemsTestFixtureState ? 1 : 0,
			SelectedItemsCategory);
		return;
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(
			TEXT("ItemsListShade"),
			FLinearColor(0.065f, 0.065f, 0.065f, 0.96f)),
		CapturePoint(584.0f, 194.8f),
		FVector2D(660.0f, 619.0f),
		3);
	UScrollBox* InventoryScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("ItemsInventoryScroll"));
	InventoryScroll->SetOrientation(Orient_Vertical);
	InventoryScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	InventoryScroll->SetAllowOverscroll(true);
	InventoryScroll->SetAnimateWheelScrolling(true);
	InventoryScroll->SetWheelScrollMultiplier(12.0f);
	PlaceOnCanvas(
		ModeCanvas,
		InventoryScroll,
		CapturePoint(584.0f, 210.5f),
		FVector2D(660.0f, 587.7f),
		5);

	const int32 RowCount = FMath::DivideAndRoundUp(VisibleStacks.Num(), 4);
	for (int32 RowIndex = 0; RowIndex < RowCount; ++RowIndex)
	{
		UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("ItemsInventoryRow_%d"), RowIndex));
		for (int32 ColumnIndex = 0; ColumnIndex < 4; ++ColumnIndex)
		{
			const int32 LocalIndex = RowIndex * 4 + ColumnIndex;
			if (!VisibleStacks.IsValidIndex(LocalIndex))
			{
				break;
			}
			const FItemsPreviewStack& Stack = VisibleStacks[LocalIndex];
			const FRokUnityEconomyItem& Item = Stack.Item;
			UCanvasPanel* Cell = WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(),
				*FString::Printf(TEXT("ItemsCell_%d"), LocalIndex));
			if (LocalIndex == SelectedItemsIndex)
			{
				PlaceOnCanvas(
					Cell,
					MakeAtlasImage(
						*FString::Printf(TEXT("ItemsCellSelect_%d"), LocalIndex),
						Item0AtlasPath,
						ItemSelectedRect,
						Item0AtlasSize,
						FVector2D(142.0f)),
					FVector2D::ZeroVector,
					FVector2D(142.0f),
					0);
			}
			PlaceOnCanvas(
				Cell,
				MakeAtlasImage(
					*FString::Printf(TEXT("ItemsCellQuality_%d"), LocalIndex),
					Item0AtlasPath,
					ResolveBagQualityRect(Item.Quality),
					Item0AtlasSize,
					FVector2D(130.0f)),
				FVector2D(6.0f),
				FVector2D(130.0f),
				1);

			const TCHAR* IconAtlasPath = nullptr;
			FIntRect IconRect;
			FIntPoint IconAtlasSize;
			if (ResolveEconomyItemIcon(Item, IconAtlasPath, IconRect, IconAtlasSize))
			{
				PlaceOnCanvas(
					Cell,
					MakeAtlasImage(
						*FString::Printf(TEXT("ItemsCellIcon_%d"), LocalIndex),
						IconAtlasPath,
						IconRect,
						IconAtlasSize,
						FVector2D(110.0f)),
					FVector2D(16.0f, 20.0f),
					FVector2D(110.0f),
					2);
			}

			const FText TopLabel = ResolveEconomyItemTopLabel(Item);
			if (!TopLabel.IsEmpty())
			{
				PlaceOnCanvas(
					Cell,
					MakeAtlasImage(
						*FString::Printf(TEXT("ItemsCellTop_%d"), LocalIndex),
						CommonAtlasPath,
						ItemTopRect,
						CommonAtlasSize,
						FVector2D(120.0f, 27.0f)),
					FVector2D(11.0f, 7.0f),
					FVector2D(120.0f, 27.0f),
					3);
				UTextBlock* TopText = MakeScreenText(
					*FString::Printf(TEXT("ItemsCellTopText_%d"), LocalIndex),
					15,
					FLinearColor(0.93f, 0.91f, 0.81f, 1.0f),
					false,
					ETextJustify::Center);
				TopText->SetText(TopLabel);
				PlaceOnCanvas(
					Cell,
					TopText,
					FVector2D(11.0f, 8.0f),
					FVector2D(120.0f, 25.0f),
					4);
			}

			UTextBlock* Quantity = MakeScreenText(
				*FString::Printf(TEXT("ItemsCellQuantity_%d"), LocalIndex),
				16,
				FLinearColor::White,
				true,
				ETextJustify::Right);
			Quantity->SetText(FText::AsNumber(Stack.Overlay));
			PlaceOnCanvas(
				Cell,
				Quantity,
				FVector2D(77.0f, 105.0f),
				FVector2D(51.0f, 26.0f),
				4);
			PlaceOnCanvas(
				Cell,
				MakeActionButton(
					*FString::Printf(TEXT("ItemsCellHit_%d"), LocalIndex),
					5000 + LocalIndex,
					nullptr,
					FIntRect(),
					FIntPoint::ZeroValue,
					FVector2D(142.0f)),
				FVector2D::ZeroVector,
				FVector2D(142.0f),
				8);
			PlaceOnCanvas(
				Row,
				Cell,
				FVector2D(8.0f + ColumnIndex * 160.0f, 4.0f),
				FVector2D(142.0f),
				1);
		}
		InventoryScroll->AddChild(WrapFixedSize(
			Row,
			*FString::Printf(TEXT("ItemsInventoryRowSize_%d"), RowIndex),
			FVector2D(660.0f, 150.0f)));
	}

	const FItemsPreviewStack& SelectedStack = VisibleStacks[SelectedItemsIndex];
	const FRokUnityEconomyItem& SelectedItem = SelectedStack.Item;
	UCanvasPanel* DetailPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("ItemsDetailPanel"));
	PlaceOnCanvas(
		DetailPanel,
		MakeSolidBorder(
			TEXT("ItemsDetailFill"),
			FLinearColor(0.72f, 0.70f, 0.58f, 1.0f)),
		FVector2D::ZeroVector,
		FVector2D(412.0f, 616.0f),
		0);
	PlaceOnCanvas(
		DetailPanel,
		MakeAtlasBorder(
			TEXT("ItemsDetailFrame"),
			FrameAtlasPath,
			ActivityFrameRect,
			FrameAtlasSize,
			FVector2D(412.0f, 616.0f),
			ActivityFrameMargin),
		FVector2D::ZeroVector,
		FVector2D(412.0f, 616.0f),
		1);
	PlaceOnCanvas(
		DetailPanel,
		MakeAtlasImage(
			TEXT("ItemsDetailQuality"),
			Item0AtlasPath,
			ResolveBagQualityRect(SelectedItem.Quality),
			Item0AtlasSize,
			FVector2D(142.0f)),
		FVector2D(135.0f, 28.0f),
		FVector2D(142.0f),
		2);
	const TCHAR* DetailIconAtlasPath = nullptr;
	FIntRect DetailIconRect;
	FIntPoint DetailIconAtlasSize;
	if (ResolveEconomyItemIcon(
		SelectedItem,
		DetailIconAtlasPath,
		DetailIconRect,
		DetailIconAtlasSize))
	{
		PlaceOnCanvas(
			DetailPanel,
			MakeAtlasImage(
				TEXT("ItemsDetailIcon"),
				DetailIconAtlasPath,
				DetailIconRect,
				DetailIconAtlasSize,
				FVector2D(122.0f)),
			FVector2D(145.0f, 38.0f),
			FVector2D(122.0f),
			3);
	}
	const FText DetailTopLabel = ResolveEconomyItemTopLabel(SelectedItem);
	if (!DetailTopLabel.IsEmpty())
	{
		PlaceOnCanvas(DetailPanel, MakeAtlasImage(TEXT("ItemsDetailTop"), CommonAtlasPath, ItemTopRect, CommonAtlasSize, FVector2D(120.0f, 27.0f)), FVector2D(146.0f, 31.0f), FVector2D(120.0f, 27.0f), 4);
		UTextBlock* DetailTop = MakeScreenText(TEXT("ItemsDetailTopText"), 16, FLinearColor(0.93f, 0.91f, 0.81f, 1.0f), false, ETextJustify::Center);
		DetailTop->SetText(DetailTopLabel);
		PlaceOnCanvas(DetailPanel, DetailTop, FVector2D(146.0f, 32.0f), FVector2D(120.0f, 25.0f), 5);
	}

	UTextBlock* ItemName = MakeScreenText(TEXT("ItemsDetailName"), 25, FLinearColor(0.22f, 0.19f, 0.12f, 1.0f), true, ETextJustify::Center);
	ItemName->SetText(FText::FromString(
		SelectedItem.DisplayNameZhCn.IsEmpty()
			? FString::Printf(TEXT("道具 %d"), SelectedItem.Id)
			: SelectedItem.DisplayNameZhCn));
	PlaceOnCanvas(DetailPanel, ItemName, FVector2D(31.0f, 185.0f), FVector2D(350.0f, 42.0f), 4);
	PlaceOnCanvas(DetailPanel, MakeSolidBorder(TEXT("ItemsDescriptionShade"), FLinearColor(0.31f, 0.30f, 0.25f, 0.52f)), FVector2D(28.0f, 235.0f), FVector2D(356.0f, 144.0f), 2);
	UTextBlock* Description = MakeScreenText(TEXT("ItemsDescription"), 19, FLinearColor::White, false, ETextJustify::Center);
	Description->SetText(FText::FromString(ResolveEconomyItemDescription(SelectedItem)));
	Description->SetAutoWrapText(true);
	PlaceOnCanvas(DetailPanel, Description, FVector2D(45.0f, 255.0f), FVector2D(322.0f, 104.0f), 4);

	UTextBlock* Owned = MakeScreenText(
		TEXT("ItemsOwned"),
		18,
		FLinearColor(0.80f, 0.78f, 0.70f, 1.0f),
		false,
		ETextJustify::Center);
	Owned->SetText(FText::Format(
		NSLOCTEXT("RokItems", "OwnedFormat", "拥有：{0}"),
		FText::AsNumber(SelectedStack.Overlay)));
	PlaceOnCanvas(DetailPanel, Owned, FVector2D(81.0f, 401.0f), FVector2D(250.0f, 30.0f), 4);

	if (SelectedItem.bBatchUse)
	{
		URokUnityIndexedButton* Minus = MakeActionButton(TEXT("ItemsMinus"), 540, CommonAtlasPath, ItemMinusRect, CommonAtlasSize, FVector2D(46.0f), FText::FromString(TEXT("−")), 23);
		Minus->SetIsEnabled(SelectedItemsUseCount > 1);
		PlaceOnCanvas(DetailPanel, Minus, FVector2D(29.0f, 442.0f), FVector2D(46.0f), 6);
		PlaceOnCanvas(DetailPanel, MakeSolidBorder(TEXT("ItemsQuantityBack"), FLinearColor(0.55f, 0.53f, 0.44f, 0.75f)), FVector2D(82.0f, 444.0f), FVector2D(130.0f, 43.0f), 4);
		UTextBlock* Quantity = MakeScreenText(TEXT("ItemsQuantity"), 22, FLinearColor::White, true, ETextJustify::Center);
		Quantity->SetText(FText::AsNumber(SelectedItemsUseCount));
		PlaceOnCanvas(DetailPanel, Quantity, FVector2D(82.0f, 448.0f), FVector2D(130.0f, 34.0f), 6);
		URokUnityIndexedButton* Plus = MakeActionButton(TEXT("ItemsPlus"), 541, CommonAtlasPath, ItemPlusRect, CommonAtlasSize, FVector2D(46.0f), FText::FromString(TEXT("+")), 23);
		Plus->SetIsEnabled(SelectedItemsUseCount < FMath::Min<int64>(SelectedStack.Overlay, 9999));
		PlaceOnCanvas(DetailPanel, Plus, FVector2D(218.0f, 442.0f), FVector2D(46.0f), 6);
		URokUnityIndexedButton* Max = MakeActionButton(TEXT("ItemsMax"), 542, CommonAtlasPath, ItemMaxRect, CommonAtlasSize, FVector2D(87.0f, 51.0f), NSLOCTEXT("RokItems", "Max", "最大"), 19);
		Max->SetIsEnabled(SelectedItemsUseCount < FMath::Min<int64>(SelectedStack.Overlay, 9999));
		PlaceOnCanvas(DetailPanel, Max, FVector2D(270.0f, 439.0f), FVector2D(87.0f, 51.0f), 6);
	}
	if (SelectedItem.ButtonLocaleId > 0)
	{
		PlaceOnCanvas(
			DetailPanel,
			MakeActionButton(TEXT("ItemsUse"), 543, CommonAtlasPath, MediumBlueButtonRect, CommonAtlasSize, FVector2D(202.0f, 81.0f), NSLOCTEXT("RokItems", "Use", "使用"), 28, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f)),
			FVector2D(105.0f, 513.0f),
			FVector2D(202.0f, 81.0f),
			6);
	}
	else
	{
		UTextBlock* RoutedUse = MakeScreenText(
			TEXT("ItemsRoutedUse"),
			17,
			FLinearColor(0.26f, 0.23f, 0.17f, 1.0f),
			false,
			ETextJustify::Center);
		RoutedUse->SetText(NSLOCTEXT(
			"RokItems",
			"RoutedUseLabel",
			"请在对应功能中使用"));
		PlaceOnCanvas(
			DetailPanel,
			RoutedUse,
			FVector2D(86.0f, 534.0f),
			FVector2D(240.0f, 40.0f),
			6);
	}
	PlaceOnCanvas(
		ModeCanvas,
		DetailPanel,
		CapturePoint(178.0f, 192.3f),
		FVector2D(412.0f, 616.0f),
		5);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Rok items screen: inventory=fixture category=%d stacks=%d selected_item=%d overlay=%lld."),
		SelectedItemsCategory,
		VisibleStacks.Num(),
		SelectedStack.ItemId,
		static_cast<long long>(SelectedStack.Overlay));
}

void URokUnityFeatureScreenWidget::BuildVip()
{
	const int32 PreviewLevel = FMath::Clamp(SelectedVipPreviewLevel, 0, VipLevelCount - 1);
	FRokUnityRuntimePlayerProfileState PlayerProfileState;
	if (const URokUnityRuntimeStateSubsystem* RuntimeState =
		GetRuntimeStateSubsystem())
	{
		PlayerProfileState = RuntimeState->GetPlayerProfileState();
	}
	const bool bHasAccountVipLevel = bVipTestFixtureState
		|| PlayerProfileState.bHasVipLevel;
	const int32 CurrentVipLevel = bVipTestFixtureState
		? 0
		: PlayerProfileState.VipLevel;
	// The current runtime bootstrap only owns an optional VIP level. Points,
	// login streaks and reward flags still require the server role snapshot.
	const bool bHasVipSessionState = bVipTestFixtureState;
	int32 FixtureTodayPoints = 40;
	FRokUnityCatalogVipLevel VipLevelConfig;
	TArray<FRokUnityCatalogVipAttribute> VipAttributes;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const URokUnityGameplayCatalogSubsystem* Catalog = GameInstance->GetSubsystem<URokUnityGameplayCatalogSubsystem>())
		{
			Catalog->GetVipLevel(PreviewLevel, VipLevelConfig);
			VipAttributes = Catalog->GetVipAttributesForLevel(PreviewLevel);
			FRokUnityCatalogVipDayPoint FirstLoginDayPoint;
			if (Catalog->GetVipDayPoint(1, FirstLoginDayPoint))
			{
				FixtureTodayPoints = FirstLoginDayPoint.Point;
			}
		}
	}
	if (bVipTestFixtureState && PreviewLevel == 0)
	{
		// Source-backed first-login fixture for tests that construct this
		// widget without a GameInstance/catalog subsystem.
		if (VipLevelConfig.Point <= 0)
		{
			VipLevelConfig.Point = 200;
		}
		if (VipLevelConfig.PriceDisplay.IsEmpty())
		{
			VipLevelConfig.PriceDisplay = TEXT("$0.99");
		}
	}

	BuildSplitWindowFrame(
		CapturePoint(160.0f, 100.0f),
		FVector2D(1120.0f, 700.0f),
		NSLOCTEXT("RokVip", "Title", "VIP"),
		CapturePoint(1206.0f, 91.0f));

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipContentShade"), FLinearColor(0.10f, 0.10f, 0.10f, 0.98f)),
		CapturePoint(208.0f, 162.0f),
		FVector2D(1024.0f, 612.0f),
		2);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(
			TEXT("VipCurrentLevelBadge"),
			VipLevelAtlasPath,
			VipLevelIconRects[FMath::Clamp(CurrentVipLevel, 0, VipLevelCount - 1)],
			VipLevelAtlasSize,
			FVector2D(138.0f)),
		CapturePoint(1091.0f, 168.0f),
		FVector2D(138.0f),
		5);
	UTextBlock* CurrentLevelText = MakeScreenText(TEXT("VipCurrentLevelText"), 42, FLinearColor(0.34f, 0.33f, 0.29f, 1.0f), false, ETextJustify::Center);
	CurrentLevelText->SetText(bHasAccountVipLevel
		? FText::AsNumber(CurrentVipLevel)
		: FText::FromString(TEXT("--")));
	PlaceOnCanvas(ModeCanvas, CurrentLevelText, CapturePoint(1130.0f, 207.0f), FVector2D(60.0f), 6);

	PlaceOnCanvas(
		ModeCanvas,
		MakeActionButton(TEXT("VipHelp"), 742, CommonAtlasPath, VipHelpRect, CommonAtlasSize, FVector2D(30.0f)),
		CapturePoint(1042.0f, 182.0f),
		FVector2D(30.0f),
		8);
	UTextBlock* TodayPoints = MakeScreenText(TEXT("VipTodayPoints"), 20, FLinearColor(0.92f, 0.92f, 0.9f, 1.0f));
	TodayPoints->SetText(bHasVipSessionState
		? FText::Format(
			NSLOCTEXT("RokVip", "TodayPointsFormat", "今日VIP点数：{0}"),
			FText::AsNumber(FixtureTodayPoints))
		: NSLOCTEXT("RokVip", "TodayPointsUnavailable", "今日VIP点数：未同步"));
	PlaceOnCanvas(ModeCanvas, TodayPoints, CapturePoint(713.0f, 170.0f), FVector2D(316.0f, 34.0f), 6);

	UTextBlock* LoginDays = MakeScreenText(TEXT("VipLoginDays"), 18, FLinearColor(0.92f, 0.92f, 0.9f, 1.0f));
	LoginDays->SetText(bHasVipSessionState
		? NSLOCTEXT("RokVip", "FirstLoginDays", "连续登录天数： 1")
		: NSLOCTEXT("RokVip", "LoginDaysUnavailable", "连续登录天数：未同步"));
	PlaceOnCanvas(ModeCanvas, LoginDays, CapturePoint(379.0f, 170.0f), FVector2D(349.0f, 32.0f), 6);

	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(TEXT("VipCurrentPointMarker"), CommonAtlasPath, VipCurrentPointRect, CommonAtlasSize, FVector2D(62.0f)),
		CapturePoint(1020.0f, 212.0f),
		FVector2D(62.0f),
		7);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipProgressBackground"), FLinearColor(0.075f, 0.075f, 0.075f, 0.98f)),
		CapturePoint(436.0f, 233.0f),
		FVector2D(599.0f, 23.0f),
		5);
	UTextBlock* ProgressText = MakeScreenText(TEXT("VipProgressText"), 18, FLinearColor::White, false, ETextJustify::Center);
	ProgressText->SetText(bHasVipSessionState
		? FText::Format(
			NSLOCTEXT("RokVip", "FirstLoginProgressFormat", "0/{0}"),
			FText::AsNumber(FMath::Max(0, VipLevelConfig.Point)))
		: NSLOCTEXT("RokVip", "ProgressUnavailable", "VIP进度未同步"));
	PlaceOnCanvas(ModeCanvas, ProgressText, CapturePoint(436.0f, 229.0f), FVector2D(599.0f, 30.0f), 7);
	URokUnityIndexedButton* AddPointsButton = MakeActionButton(
		TEXT("VipAddPoints"),
		741,
		CommonAtlasPath,
		VipAddRect,
		CommonAtlasSize,
		FVector2D(50.0f));
	AddPointsButton->SetIsEnabled(bHasVipSessionState);
	PlaceOnCanvas(
		ModeCanvas,
		AddPointsButton,
		CapturePoint(382.0f, 219.0f),
		FVector2D(50.0f),
		8);

	URokUnityIndexedButton* DailyPointChestButton = MakeActionButton(
		TEXT("VipDailyPointChest"),
		740,
		TreasureBoxAtlasPath,
		VipDailyPointChestRect,
		TreasureBoxAtlasSize,
		FVector2D(141.0f, 115.0f));
	DailyPointChestButton->SetIsEnabled(bHasVipSessionState);
	PlaceOnCanvas(
		ModeCanvas,
		DailyPointChestButton,
		CapturePoint(224.0f, 184.0f),
		FVector2D(141.0f, 115.0f),
		7);
	if (bHasVipSessionState)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(TEXT("VipDailyPointRedPoint"), CommonAtlasPath, RedPointRect, CommonAtlasSize, FVector2D(18.0f)),
			CapturePoint(256.0f, 203.0f),
			FVector2D(18.0f),
			9);
	}
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipDailyPointLabelShade"), FLinearColor(0.08f, 0.08f, 0.08f, 0.82f)),
		CapturePoint(245.0f, 255.0f),
		FVector2D(100.0f, 25.0f),
		7);
	UTextBlock* DailyPointLabel = MakeScreenText(TEXT("VipDailyPointLabel"), 17, FLinearColor::White, false, ETextJustify::Center);
	DailyPointLabel->SetText(bHasVipSessionState
		? NSLOCTEXT("RokVip", "Available", "可领取")
		: NSLOCTEXT("RokVip", "AwaitingLogin", "待登录同步"));
	PlaceOnCanvas(ModeCanvas, DailyPointLabel, CapturePoint(247.0f, 255.0f), FVector2D(100.0f, 25.0f), 8);

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipMainPanel"), FLinearColor(0.075f, 0.075f, 0.075f, 0.98f)),
		CapturePoint(235.0f, 316.0f),
		FVector2D(973.0f, 419.0f),
		3);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasBorder(TEXT("VipMainPanelFrame"), CommonAtlasPath, TaskRowRect, CommonAtlasSize, FVector2D(973.0f, 419.0f), TaskRowMargin),
		CapturePoint(235.0f, 316.0f),
		FVector2D(973.0f, 419.0f),
		4);

	UTextBlock* BenefitTitle = MakeScreenText(TEXT("VipBenefitTitle"), 21, FLinearColor::White, false, ETextJustify::Center);
	BenefitTitle->SetText(FText::Format(NSLOCTEXT("RokVip", "BenefitTitleFormat", "VIP {0}专属增益"), FText::AsNumber(PreviewLevel)));
	PlaceOnCanvas(ModeCanvas, BenefitTitle, CapturePoint(781.0f, 326.0f), FVector2D(340.0f, 50.0f), 6);
	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipBenefitsPanel"), FLinearColor(0.035f, 0.035f, 0.035f, 0.96f)),
		CapturePoint(727.0f, 379.0f),
		FVector2D(448.0f, 323.0f),
		5);

	UScrollBox* BenefitsScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("VipBenefitsScroll"));
	BenefitsScroll->SetOrientation(Orient_Vertical);
	BenefitsScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	BenefitsScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	PlaceOnCanvas(ModeCanvas, BenefitsScroll, CapturePoint(733.0f, 392.0f), FVector2D(436.0f, 297.0f), 7);

	for (int32 Index = 0; Index < VipAttributes.Num(); ++Index)
	{
		const FRokUnityCatalogVipAttribute& Attribute = VipAttributes[Index];
		UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("VipBenefitRow_%d"), Index));
		USizeBox* RowSize = WrapFixedSize(
			Row,
			*FString::Printf(TEXT("VipBenefitRowSize_%d"), Index),
			FVector2D(410.0f, 43.0f));
		BenefitsScroll->AddChild(RowSize);

		float TextX = 8.0f;
		if (Attribute.bNew)
		{
			PlaceOnCanvas(
				Row,
				MakeAtlasImage(*FString::Printf(TEXT("VipBenefitNew_%d"), Index), CommonAtlasPath, NewRect, CommonAtlasSize, FVector2D(43.0f, 45.0f)),
				FVector2D(0.0f, -2.0f),
				FVector2D(43.0f, 45.0f),
				1);
			UTextBlock* NewLabel = MakeScreenText(*FString::Printf(TEXT("VipBenefitNewLabel_%d"), Index), 14, FLinearColor::White, true, ETextJustify::Center);
			NewLabel->SetText(NSLOCTEXT("RokVip", "New", "新"));
			PlaceOnCanvas(Row, NewLabel, FVector2D(4.0f, 7.0f), FVector2D(34.0f, 24.0f), 2);
			TextX = 47.0f;
		}

		UTextBlock* Benefit = MakeScreenText(*FString::Printf(TEXT("VipBenefit_%d"), Index), 19, FLinearColor(0.93f, 0.93f, 0.91f, 1.0f));
		Benefit->SetText(FText::FromString(Attribute.DisplayTextZhCn));
		PlaceOnCanvas(Row, Benefit, FVector2D(TextX, 6.0f), FVector2D(354.0f - TextX, 31.0f), 2);
	}

	UTextBlock* ChestTitle = MakeScreenText(TEXT("VipChestTitle"), 21, FLinearColor::White, false, ETextJustify::Center);
	ChestTitle->SetText(FText::Format(NSLOCTEXT("RokVip", "ChestTitleFormat", "VIP {0}专属宝箱"), FText::AsNumber(PreviewLevel)));
	PlaceOnCanvas(ModeCanvas, ChestTitle, CapturePoint(321.0f, 325.0f), FVector2D(340.0f, 50.0f), 6);

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipFreeChestPanel"), FLinearColor(0.035f, 0.035f, 0.035f, 0.96f)),
		CapturePoint(291.0f, 380.0f),
		FVector2D(400.0f, 145.0f),
		5);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(TEXT("VipDailyFreeTitle"), VipPlayerAtlasPath, VipDailyTitleRect, VipPlayerAtlasSize, FVector2D(410.0f, 51.0f)),
		CapturePoint(286.0f, 369.0f),
		FVector2D(410.0f, 51.0f),
		7);
	UTextBlock* DailyFreeText = MakeScreenText(TEXT("VipDailyFreeText"), 19, FLinearColor::White, false, ETextJustify::Center);
	DailyFreeText->SetText(NSLOCTEXT("RokVip", "DailyFree", "每日免费"));
	PlaceOnCanvas(ModeCanvas, DailyFreeText, CapturePoint(286.0f, 378.0f), FVector2D(410.0f, 30.0f), 8);
	UImage* FreeChest = MakeAtlasImage(TEXT("VipFreeChest"), TreasureBoxAtlasPath, VipFreeChestRect, TreasureBoxAtlasSize, FVector2D(92.0f, 110.0f));
	FreeChest->SetRenderTransformPivot(FVector2D(0.5f));
	FreeChest->SetRenderTransformAngle(90.0f);
	PlaceOnCanvas(ModeCanvas, FreeChest, CapturePoint(578.0f, 410.0f), FVector2D(92.0f, 110.0f), 7);
	URokUnityIndexedButton* FreeChestHitButton = MakeActionButton(
		TEXT("VipFreeChestHit"),
		743,
		nullptr,
		FIntRect(),
		FIntPoint::ZeroValue,
		FVector2D(109.2f, 77.3f));
	FreeChestHitButton->SetIsEnabled(bHasVipSessionState);
	PlaceOnCanvas(
		ModeCanvas,
		FreeChestHitButton,
		CapturePoint(578.1f, 437.55f),
		FVector2D(109.2f, 77.3f),
		9);
	URokUnityIndexedButton* ClaimFreeChestButton = MakeActionButton(
		TEXT("VipClaimFreeChest"),
		743,
		CommonAtlasPath,
		bHasVipSessionState ? MiniGreenButtonRect : MiniBlueButtonRect,
		CommonAtlasSize,
		FVector2D(138.0f, 54.0f),
		bHasVipSessionState
			? NSLOCTEXT("RokVip", "Claim", "领取")
			: NSLOCTEXT("RokVip", "ClaimRequiresLogin", "需登录"),
		22,
		FLinearColor::White,
		ESlateBrushDrawType::Box,
		bHasVipSessionState ? MiniGreenMargin : MiniBlueMargin);
	ClaimFreeChestButton->SetIsEnabled(bHasVipSessionState);
	PlaceOnCanvas(
		ModeCanvas,
		ClaimFreeChestButton,
		CapturePoint(392.8f, 447.35f),
		FVector2D(135.0f, 50.5f),
		9);
	if (bHasVipSessionState)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeAtlasImage(TEXT("VipFreeChestRedPoint"), CommonAtlasPath, RedPointRect, CommonAtlasSize, FVector2D(22.0f)),
			CapturePoint(521.0f, 423.0f),
			FVector2D(22.0f),
			10);
	}

	PlaceOnCanvas(
		ModeCanvas,
		MakeSolidBorder(TEXT("VipSpecialChestPanel"), FLinearColor(0.035f, 0.035f, 0.035f, 0.96f)),
		CapturePoint(291.0f, 547.0f),
		FVector2D(400.0f, 145.0f),
		5);
	PlaceOnCanvas(
		ModeCanvas,
		MakeAtlasImage(TEXT("VipSpecialTitle"), VipPlayerAtlasPath, VipSpecialTitleRect, VipPlayerAtlasSize, FVector2D(410.0f, 51.0f)),
		CapturePoint(286.0f, 536.0f),
		FVector2D(410.0f, 51.0f),
		7);
	UTextBlock* SpecialText = MakeScreenText(TEXT("VipSpecialText"), 19, FLinearColor::White, false, ETextJustify::Center);
	SpecialText->SetText(NSLOCTEXT("RokVip", "SpecialPrivilege", "特别尊享"));
	PlaceOnCanvas(ModeCanvas, SpecialText, CapturePoint(286.0f, 545.0f), FVector2D(410.0f, 30.0f), 8);
	UImage* SpecialChest = MakeAtlasImage(TEXT("VipSpecialChest"), TreasureBoxAtlasPath, VipSpecialChestRect, TreasureBoxAtlasSize, FVector2D(94.0f, 112.0f));
	SpecialChest->SetRenderTransformPivot(FVector2D(0.5f));
	SpecialChest->SetRenderTransformAngle(90.0f);
	PlaceOnCanvas(ModeCanvas, SpecialChest, CapturePoint(578.0f, 571.0f), FVector2D(94.0f, 112.0f), 7);
	URokUnityIndexedButton* SpecialChestHitButton = MakeActionButton(
		TEXT("VipSpecialChestHit"),
		744,
		nullptr,
		FIntRect(),
		FIntPoint::ZeroValue,
		FVector2D(109.2f, 77.3f));
	SpecialChestHitButton->SetIsEnabled(bHasVipSessionState);
	PlaceOnCanvas(
		ModeCanvas,
		SpecialChestHitButton,
		CapturePoint(578.1f, 611.0f),
		FVector2D(109.2f, 77.3f),
		9);
	URokUnityIndexedButton* BuySpecialChestButton = MakeActionButton(
		TEXT("VipBuySpecialChest"),
		744,
		CommonAtlasPath,
		MediumGoldButtonRect,
		CommonAtlasSize,
		FVector2D(138.0f, 58.0f),
		bHasVipSessionState
			? FText::FromString(VipLevelConfig.PriceDisplay)
			: NSLOCTEXT("RokVip", "PurchaseRequiresLogin", "需登录"),
		21,
		FLinearColor::White,
		ESlateBrushDrawType::Box,
		FMargin(0.30f, 0.0f, 0.30f, 0.0f));
	BuySpecialChestButton->SetIsEnabled(bHasVipSessionState);
	PlaceOnCanvas(
		ModeCanvas,
		BuySpecialChestButton,
		CapturePoint(392.8f, 614.65f),
		FVector2D(135.0f, 50.5f),
		9);

	if (PreviewLevel > 0)
	{
		URokUnityIndexedButton* Previous = MakeActionButton(TEXT("VipPreviousPage"), 700, CommonAtlasPath, VipPageArrowRect, CommonAtlasSize, FVector2D(110.0f, 276.0f));
		Previous->SetRenderTransformPivot(FVector2D(0.5f));
		Previous->SetRenderScale(FVector2D(-1.0f, 1.0f));
		PlaceOnCanvas(ModeCanvas, Previous, CapturePoint(130.0f, 386.0f), FVector2D(110.0f, 276.0f), 9);
	}
	if (PreviewLevel + 1 < VipLevelCount)
	{
		PlaceOnCanvas(
			ModeCanvas,
			MakeActionButton(TEXT("VipNextPage"), 701, CommonAtlasPath, VipPageArrowRect, CommonAtlasSize, FVector2D(110.0f, 276.0f)),
			CapturePoint(1203.0f, 386.0f),
			FVector2D(110.0f, 276.0f),
			9);
	}

	if (bVipTestFixtureState)
	{
		// VIP points, streaks and chest flags are server role state; the
		// fixture preview must stay explicitly labeled, not read as account data.
		UTextBlock* VipFixtureTag = MakeScreenText(TEXT("VipFixtureTag"), 15, FLinearColor(0.62f, 0.62f, 0.58f, 1.0f), false);
		VipFixtureTag->SetText(NSLOCTEXT("RokVip", "FixtureTag", "VIP · 本地参考态（非账号状态）"));
		PlaceOnCanvas(ModeCanvas, VipFixtureTag, CapturePoint(220.0f, 745.0f), FVector2D(420.0f, 26.0f), 6);
	}

	if (bVipDailyPointHelpVisible)
	{
		BuildVipDailyPointHelpTip();
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok VIP screen: account_level=%s current_level=%d preview_level=%d fixture=%d session_state=%d authority=%s."),
		bHasAccountVipLevel ? TEXT("known") : TEXT("unknown"),
		bHasAccountVipLevel ? CurrentVipLevel : INDEX_NONE,
		PreviewLevel,
		bVipTestFixtureState ? 1 : 0,
		bHasVipSessionState ? 1 : 0,
		bVipTestFixtureState ? TEXT("local_fixture") : TEXT("server"));
}

void URokUnityFeatureScreenWidget::BuildVipDailyPointHelpTip()
{
	if (!ModeCanvas || !bVipDailyPointHelpVisible)
	{
		return;
	}
	FText HelpText;
	const URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
	if (!Catalog || !Catalog->GetVipDailyPointHelpText(HelpText))
	{
		bVipDailyPointHelpVisible = false;
		return;
	}

	VipDailyPointHelpTipRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("VipDailyPointHelpTipRoot"));
	VipDailyPointHelpTipRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlaceOnCanvas(ModeCanvas, VipDailyPointHelpTipRoot, FVector2D::ZeroVector,
		CurrentExpandedDesignSize, 100);

	const FVector2D Anchor = CapturePoint(1057.0f, 197.0f);
	UTextBlock* Body = MakeScreenText(TEXT("VipDailyPointHelpTipText"), 14,
		FLinearColor(0.13725491f, 0.13725491f, 0.1254902f, 1.0f), false, ETextJustify::Right);
	Body->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18.0f * 72.0f / 96.0f));
	Body->SetShadowOffset(FVector2D::ZeroVector);
	Body->SetText(HelpText);
	Body->SetAutoWrapText(false);
	// Keep the newly built Slate widget alive until it is placed on the canvas.
	const TSharedRef<SWidget> BodySlate = Body->TakeWidget();
	Body->ForceLayoutPrepass();
	const float NaturalBodyWidth = BodySlate->GetDesiredSize().X;
	if (!FMath::IsFinite(NaturalBodyWidth) || NaturalBodyWidth <= 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("Rok VIP daily point tip text Slate width is invalid."));
		return;
	}
	const float BodyWidth = FMath::Min(550.0f, NaturalBodyWidth);
	Body->SetWrapTextAt(BodyWidth);
	Body->SetAutoWrapText(true);
	Body->SetClipping(EWidgetClipping::ClipToBounds);
	Body->ForceLayoutPrepass();
	const float WrappedBodyHeight = BodySlate->GetDesiredSize().Y;
	if (!FMath::IsFinite(WrappedBodyHeight) || WrappedBodyHeight <= 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("Rok VIP daily point tip wrapped Slate height is invalid."));
		return;
	}
	const float TipWidth = BodyWidth + 40.0f; // Unity frame adds 40px to text width.
	const float TipHeight = FMath::Max(80.0f, WrappedBodyHeight + 40.0f);
	const FVector2D TipSize(TipWidth, TipHeight);
	const float Left = FMath::Clamp(Anchor.X - TipSize.X * 0.5f,
		0.0f, FMath::Max(0.0f, CurrentExpandedDesignSize.X - TipSize.X));
	// Unity arrowUp places the arrow on the upper edge, with the body below its anchor.
	const float Top = FMath::Min(CurrentExpandedDesignSize.Y - TipHeight - 8.0f, Anchor.Y + 12.0f);
	const FVector2D Position(Left, FMath::Max(8.0f, Top));

	// Transparent hit surface keeps a click on the tip itself from dismissing it.
	PlaceOnCanvas(VipDailyPointHelpTipRoot, MakeActionButton(
		TEXT("VipDailyPointHelpTipBodyHit"), 745, nullptr, FIntRect(), FIntPoint(1, 1), TipSize),
		Position, TipSize, 1);
	PlaceOnCanvas(VipDailyPointHelpTipRoot, MakeAtlasBorder(
		TEXT("VipDailyPointHelpTipBackground"), CommonAtlasPath,
		SpriteRect(774, 276, 72, 72), CommonAtlasSize, TipSize,
		FMargin(21.0f / 72, 23.0f / 72, 21.0f / 72, 24.0f / 72)), Position, TipSize, 2);
	PlaceOnCanvas(VipDailyPointHelpTipRoot, Body, Position + FVector2D(20.0f, 20.0f),
		FVector2D(BodyWidth, TipHeight - 40.0f), 3);
	UImage* Arrow = MakeAtlasImage(TEXT("VipDailyPointHelpTipArrow"), CommonAtlasPath,
		SpriteRect(120, 137, 15, 12), CommonAtlasSize, FVector2D(15.0f, 12.0f));
	Arrow->SetRenderTransformAngle(180.0f); // UI_Pop_TextTip img_arrowSideTop.
	PlaceOnCanvas(VipDailyPointHelpTipRoot, Arrow,
		Position + FVector2D(Anchor.X - Left - 7.5f, -10.6f),
		FVector2D(15.0f, 12.0f), 4);
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

	static const TCHAR* AllianceNames[] = {TEXT("[SNOW] 雪原同盟"), TEXT("[ROK] 王国远征军"), TEXT("[CN1] 九州盟约"), TEXT("[SUN] 日曜骑士团"), TEXT("[WLD] 世界之巅")};
	static const TCHAR* AllianceLeaders[] = {TEXT("snow"), TEXT("远征者"), TEXT("九州牧"), TEXT("Sunrise"), TEXT("WorldKing")};
	static const TCHAR* AlliancePower[] = {TEXT("6,100"), TEXT("25,730"), TEXT("18,420"), TEXT("13,880"), TEXT("9,650")};
	static const TCHAR* AllianceMembers[] = {TEXT("1/50"), TEXT("42/50"), TEXT("35/50"), TEXT("27/50"), TEXT("19/50")};
	// Alliance lists are server data; fixture rows must stay explicitly
	// labeled instead of reading as account/kingdom state.
	UTextBlock* JoinFixtureTag = MakeScreenText(TEXT("AllianceJoinFixtureTag"), 15, FLinearColor(0.62f, 0.62f, 0.58f, 1.0f), false, ETextJustify::Right);
	JoinFixtureTag->SetText(NSLOCTEXT("RokAlliance", "JoinFixtureTag", "联盟列表 · 本地参考态（非账号状态）"));
	PlaceOnCanvas(ModeCanvas, JoinFixtureTag, CapturePoint(760.0f, 742.0f), FVector2D(500.0f, 26.0f), 6);
	VisibleAllianceFixtureIndices.Reset();
	const FString& SearchKey = AllianceFixtureSearchQuery;
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
	if (VisibleAllianceFixtureIndices.IsEmpty())
	{
		UTextBlock* EmptyList = MakeScreenText(TEXT("AllianceJoinFixtureEmpty"), 20,
			FLinearColor(0.78f, 0.77f, 0.72f, 1.0f), false, ETextJustify::Center);
		EmptyList->SetText(SearchKey.Len() < 3
			? NSLOCTEXT("RokAlliance", "FixtureSearchMinimum", "本地参考搜索请至少输入3个字符。")
			: NSLOCTEXT("RokAlliance", "FixtureSearchNoMatch", "本地参考列表没有匹配的联盟。"));
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
		Stats->SetText(FText::FromString(FString::Printf(TEXT("战力 %s    成员 %s    中文"), AlliancePower[Index], AllianceMembers[Index])));
		PlaceOnCanvas(ModeCanvas, Stats, CapturePoint(278.0f, Y + 43.0f), FVector2D(430.0f, 28.0f), 6);
		UTextBlock* JoinType = MakeScreenText(*FString::Printf(TEXT("AllianceJoinType_%d"), Index), 16, bSelected ? FLinearColor(0.25f, 0.22f, 0.16f, 1.0f) : FLinearColor(0.78f, 0.78f, 0.75f, 1.0f), false, ETextJustify::Right);
		JoinType->SetText(Index == 0 ? NSLOCTEXT("RokAlliance", "DirectJoin", "任何人均可加入") : NSLOCTEXT("RokAlliance", "ApplyJoin", "申请加入"));
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
		DetailLeader->SetText(FText::FromString(FString::Printf(TEXT("盟主：%s\n战力：%s\n成员：%s"), AllianceLeaders[DetailIndex], AlliancePower[DetailIndex], AllianceMembers[DetailIndex])));
		PlaceOnCanvas(ModeCanvas, DetailLeader, CapturePoint(930.0f, 442.0f), FVector2D(266.0f, 84.0f), 6);
		UTextBlock* Notice = MakeScreenText(TEXT("AllianceJoinNotice"), 17, FLinearColor(0.90f, 0.90f, 0.87f, 1.0f), false, ETextJustify::Center);
		Notice->SetText(NSLOCTEXT("RokAlliance", "AllianceNotice", "团结互助，共同建设联盟领土。"));
		Notice->SetAutoWrapText(true);
		PlaceOnCanvas(ModeCanvas, Notice, CapturePoint(920.0f, 540.0f), FVector2D(286.0f, 60.0f), 6);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceJoinConfirm"), 620, CommonAtlasPath, MediumBlueButtonRect, CommonAtlasSize, FVector2D(202.0f, 81.0f), DetailIndex == 0 ? NSLOCTEXT("RokAlliance", "JoinNow", "加入") : NSLOCTEXT("RokAlliance", "ApplyNow", "申请加入"), 26, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f)), CapturePoint(960.0f, 615.0f), FVector2D(202.0f, 81.0f), 7);
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

void URokUnityFeatureScreenWidget::BuildAllianceRankingReference()
{
	// The source root is 1440x810. pl_Total/pl_Detail are centered 1074x664
	// at anchored (0,-18). The six fixed category slots have 340x181 source
	// RectTransforms with top-left positions derived from their pinned anchors.
	BuildSplitWindowFrame(FVector2D(146.0f, 28.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAllianceRanking", "Title", "联盟排行榜"), FVector2D(1225.0f, 28.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceRankingContentShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		FVector2D(183.0f, 91.0f), FVector2D(1074.0f, 664.0f), 2);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceRankingBack"), 19710,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f),
		NSLOCTEXT("RokAllianceRanking", "Back", "返回"), 18,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		FVector2D(155.0f, 32.0f), FVector2D(138.0f, 54.0f), 8);

	TArray<FRokLeaderboardRow> Categories;
	FString LoadError;
	if (!RokAllianceRankingReadonlyCatalog::Load(Categories, LoadError))
	{
		UTextBlock* Unavailable = MakeScreenText(TEXT("AllianceRankingCatalogUnavailable"), 22,
			FLinearColor(0.88f, 0.84f, 0.76f, 1.0f), false, ETextJustify::Center);
		Unavailable->SetText(NSLOCTEXT("RokAllianceRanking", "CatalogUnavailable",
			"排行榜分类暂不可用"));
		PlaceOnCanvas(ModeCanvas, Unavailable, FVector2D(250.0f, 360.0f),
			FVector2D(940.0f, 55.0f), 6);
		UE_LOG(LogTemp, Warning, TEXT("Rok alliance ranking readonly catalog unavailable: %s"), *LoadError);
		return;
	}

	const FRokLeaderboardRow* Selected = nullptr;
	for (const FRokLeaderboardRow& Category : Categories)
	{
		if (Category.Id == SelectedAllianceRankingCategoryId)
		{
			Selected = &Category;
			break;
		}
	}
	if (Selected)
	{
		UTextBlock* CategoryTitle = MakeScreenText(TEXT("AllianceRankingDetailTitle"), 27,
			FLinearColor(0.95f, 0.89f, 0.67f, 1.0f), true, ETextJustify::Center);
		CategoryTitle->SetText(FText::FromString(Selected->Name));
		PlaceOnCanvas(ModeCanvas, CategoryTitle, FVector2D(340.0f, 170.0f),
			FVector2D(760.0f, 48.0f), 6);
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceRankingEmptyListPanel"),
			FLinearColor(0.15f, 0.15f, 0.14f, 0.98f)), FVector2D(192.915f, 227.23f),
			FVector2D(1054.17f, 531.22f), 3);
		UTextBlock* EmptyList = MakeScreenText(TEXT("AllianceRankingEmptyList"), 23,
			FLinearColor(0.87f, 0.85f, 0.79f, 1.0f), false, ETextJustify::Center);
		EmptyList->SetText(NSLOCTEXT("RokAllianceRanking", "NoRankData", "暂无排名数据"));
		PlaceOnCanvas(ModeCanvas, EmptyList, FVector2D(520.0f, 393.0f),
			FVector2D(400.0f, 60.0f), 6);
		UTextBlock* NoSelf = MakeScreenText(TEXT("AllianceRankingNoSelfData"), 17,
			FLinearColor(0.73f, 0.73f, 0.70f, 1.0f), false, ETextJustify::Center);
		NoSelf->SetText(NSLOCTEXT("RokAllianceRanking", "NoSelfData",
			"当前排名与得分未接入"));
		PlaceOnCanvas(ModeCanvas, NoSelf, FVector2D(515.0f, 470.0f),
			FVector2D(410.0f, 34.0f), 6);
		return;
	}

	UTextBlock* Boundary = MakeScreenText(TEXT("AllianceRankingOverviewBoundary"), 17,
		FLinearColor(0.83f, 0.82f, 0.75f, 1.0f), false, ETextJustify::Center);
	Boundary->SetText(NSLOCTEXT("RokAllianceRanking", "CategoryPreview",
		"六类排行榜分类预览 · 排名数据未接入"));
	PlaceOnCanvas(ModeCanvas, Boundary, FVector2D(335.0f, 115.0f),
		FVector2D(770.0f, 34.0f), 6);
	// These are the six pinned pl_Total slot rectangles, in local ID order for
	// the offline preview. Unity assigns server-response entries to slots at
	// runtime; category-to-slot order is deliberately not claimed as parity.
	static const FVector2D SourceSlots[] = {
		FVector2D(200.0f, 141.0f), FVector2D(550.0f, 141.0f),
		FVector2D(900.0f, 141.0f), FVector2D(200.0f, 385.0f),
		FVector2D(550.0f, 385.0f), FVector2D(900.0f, 385.0f),
	};
	const UTexture2D* IconAtlas = LoadUiTexture(VipPlayerAtlasPath);
	const bool bIconAtlasReady = IconAtlas && IconAtlas->GetSizeX() == VipPlayerAtlasSize.X
		&& IconAtlas->GetSizeY() == VipPlayerAtlasSize.Y;
	for (int32 Index = 0; Index < Categories.Num() && Index < UE_ARRAY_COUNT(SourceSlots); ++Index)
	{
		const FRokLeaderboardRow& Category = Categories[Index];
		const FVector2D Position = FVector2D(SourceSlots[Index].X, SourceSlots[Index].Y);
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(
			*FString::Printf(TEXT("AllianceRankingCategoryPanel_%d"), Category.Id),
			FLinearColor(0.20f, 0.19f, 0.17f, 0.98f)),
			Position, FVector2D(340.0f, 181.0f), 3);
		UTextBlock* Name = MakeScreenText(
			*FString::Printf(TEXT("AllianceRankingCategoryName_%d"), Category.Id), 22,
			FLinearColor(0.96f, 0.90f, 0.73f, 1.0f), true);
		Name->SetText(FText::FromString(Category.Name));
		PlaceOnCanvas(ModeCanvas, Name, Position + FVector2D(16.0f, 23.0f),
			FVector2D(164.0f, 43.0f), 5);
		if (bIconAtlasReady)
		{
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(
				*FString::Printf(TEXT("AllianceRankingCategoryIcon_%d"), Category.Id),
				VipPlayerAtlasPath, Category.IconRect, VipPlayerAtlasSize,
				FVector2D(148.0f, 176.0f)), Position + FVector2D(190.0f, 2.0f),
				FVector2D(148.0f, 176.0f), 5);
		}
		else
		{
			UTextBlock* MissingIcon = MakeScreenText(
				*FString::Printf(TEXT("AllianceRankingIconMissing_%d"), Category.Id), 15,
				FLinearColor(0.82f, 0.78f, 0.68f, 1.0f), false, ETextJustify::Center);
			MissingIcon->SetText(NSLOCTEXT("RokAllianceRanking", "IconMissing", "图标未接入"));
			PlaceOnCanvas(ModeCanvas, MissingIcon, Position + FVector2D(194.0f, 72.0f),
				FVector2D(135.0f, 30.0f), 5);
		}
		UTextBlock* Empty = MakeScreenText(
			*FString::Printf(TEXT("AllianceRankingCategoryEmpty_%d"), Category.Id), 15,
			FLinearColor(0.76f, 0.75f, 0.70f, 1.0f), false);
		Empty->SetText(NSLOCTEXT("RokAllianceRanking", "CategoryEmpty", "暂无排名数据"));
		PlaceOnCanvas(ModeCanvas, Empty, Position + FVector2D(16.0f, 86.0f),
			FVector2D(164.0f, 34.0f), 5);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(
			*FString::Printf(TEXT("AllianceRankingCategoryHit_%d"), Category.Id),
			19700 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue,
			FVector2D(340.0f, 181.0f)), Position, FVector2D(340.0f, 181.0f), 8);
	}
}

void URokUnityFeatureScreenWidget::BuildAllianceTerritoryReference()
{
	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAllianceTerritory", "ReferenceTitle", "联盟领土静态配置参考"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceTerritoryReferenceShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);
	UTextBlock* Boundary = MakeScreenText(TEXT("AllianceTerritoryReferenceBoundary"), 16,
		FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
	Boundary->SetText(NSLOCTEXT("RokAllianceTerritory", "ReferenceBoundary",
		"仅显示12种静态配置；联盟实际领土、拥有数量、建筑状态、坐标、产出与权限未同步。"));
	PlaceOnCanvas(ModeCanvas, Boundary, CapturePoint(198.0f, 156.0f), FVector2D(1025.0f, 34.0f), 5);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceTerritoryReferenceBack"), 695,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f),
		NSLOCTEXT("RokAllianceTerritory", "ReferenceBack", "返回"), 18,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(1090.0f, 738.0f), FVector2D(138.0f, 54.0f), 7);
	const TArray<RokAllianceTerritoryReadonly::FGroup>& Groups = RokAllianceTerritoryReadonly::GetGroups();
	const TArray<RokAllianceTerritoryReadonly::FRow>& Rows = RokAllianceTerritoryReadonly::GetRows();
	if (SelectedAllianceTerritoryType > 0)
	{
		const RokAllianceTerritoryReadonly::FRow* Selected = RokAllianceTerritoryReadonly::FindRow(SelectedAllianceTerritoryType);
		if (Selected)
		{
			PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceTerritoryReturnToDefinitions"), 19630,
				CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(164.0f, 54.0f),
				NSLOCTEXT("RokAllianceTerritory", "ReturnToDefinitions", "返回配置列表"), 18,
				FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
				CapturePoint(985.0f, 194.0f), FVector2D(164.0f, 54.0f), 7);
			UScrollBox* DetailScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("AllianceTerritoryDefinitionDetailScroll"));
			DetailScroll->SetOrientation(Orient_Vertical);
			DetailScroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
			PlaceOnCanvas(ModeCanvas, DetailScroll, CapturePoint(198.0f, 256.0f), FVector2D(990.0f, 470.0f), 6);
			USizeBox* DetailExtent = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AllianceTerritoryDefinitionDetailExtent"));
			DetailExtent->SetWidthOverride(970.0f);
			DetailExtent->SetHeightOverride(1050.0f);
			DetailScroll->AddChild(DetailExtent);
			UCanvasPanel* DetailCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AllianceTerritoryDefinitionDetailCanvas"));
			DetailExtent->AddChild(DetailCanvas);
			UTextBlock* DetailName = MakeScreenText(TEXT("AllianceTerritoryDefinitionName"), 24,
				FLinearColor(0.94f, 0.88f, 0.62f, 1.0f), true);
			DetailName->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Selected->NameLocalizationId)));
			PlaceOnCanvas(DetailCanvas, DetailName, FVector2D(12.0f, 4.0f), FVector2D(930.0f, 50.0f), 2);
			UTextBlock* DetailIconState = MakeScreenText(TEXT("AllianceTerritoryDefinitionIconState"), 17,
				FLinearColor(0.84f, 0.80f, 0.69f, 1.0f), false);
			DetailIconState->SetText(Selected->bSourceIconMissing
				? NSLOCTEXT("RokAllianceTerritory", "DetailMalformedSourceIcon", "源图标缺失")
				: (Selected->SourceIconReference[0] == 0
					? NSLOCTEXT("RokAllianceTerritory", "DetailNoSourceIcon", "源未配置图标")
					: NSLOCTEXT("RokAllianceTerritory", "DetailPhysicalIconPending", "图标未接入")));
			PlaceOnCanvas(DetailCanvas, DetailIconState, FVector2D(12.0f, 64.0f), FVector2D(930.0f, 40.0f), 2);
			UTextBlock* DetailDescription = MakeScreenText(TEXT("AllianceTerritoryDefinitionDescription"), 17,
				FLinearColor(0.86f, 0.84f, 0.78f, 1.0f), false);
			DetailDescription->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Selected->DescriptionLocalizationId)));
			DetailDescription->SetAutoWrapText(true);
			PlaceOnCanvas(DetailCanvas, DetailDescription, FVector2D(12.0f, 124.0f), FVector2D(930.0f, 210.0f), 2);
			if (Selected->DetailLocalizationId > 0)
			{
				UTextBlock* DetailRules = MakeScreenText(TEXT("AllianceTerritoryDefinitionRules"), 16,
					FLinearColor(0.86f, 0.84f, 0.78f, 1.0f), false);
				DetailRules->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Selected->DetailLocalizationId)));
				DetailRules->SetAutoWrapText(true);
				PlaceOnCanvas(DetailCanvas, DetailRules, FVector2D(12.0f, 354.0f), FVector2D(930.0f, 680.0f), 2);
			}
			return;
		}
	}
	float ContentHeight = 0.0f;
	for (const RokAllianceTerritoryReadonly::FGroup& Group : Groups)
	{
		ContentHeight += 86.0f + 10.0f; // Unity GetItemSize header and ListView spacing.
		if (ExpandedAllianceTerritoryGroup == Group.Index)
		{
			ContentHeight += (Group.Index == 3 ? 290.0f : 370.0f) + 10.0f;
		}
	}
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("AllianceTerritoryReferenceScroll"));
	Scroll->SetOrientation(Orient_Vertical);
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	Scroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(ModeCanvas, Scroll, CapturePoint(194.0f, 210.0f), FVector2D(1052.0f, 518.0f), 4);
	USizeBox* Extent = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AllianceTerritoryReferenceExtent"));
	Extent->SetWidthOverride(1052.0f);
	Extent->SetHeightOverride(ContentHeight);
	Scroll->AddChild(Extent);
	UCanvasPanel* List = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AllianceTerritoryReferenceList"));
	List->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Extent->AddChild(List);
	float Y = 0.0f;
	for (const RokAllianceTerritoryReadonly::FGroup& Group : Groups)
	{
		const bool bExpanded = ExpandedAllianceTerritoryGroup == Group.Index;
		UCanvasPanel* Header = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("AllianceTerritoryGroup_%d"), Group.Index));
		PlaceOnCanvas(List, Header, FVector2D(0.0f, Y), FVector2D(1052.0f, 86.0f), 1);
		PlaceOnCanvas(Header, MakeSolidBorder(*FString::Printf(TEXT("AllianceTerritoryGroupBg_%d"), Group.Index),
			bExpanded ? FLinearColor(0.31f, 0.27f, 0.20f, 0.98f) : FLinearColor(0.19f, 0.19f, 0.18f, 0.98f)),
			FVector2D::ZeroVector, FVector2D(1052.0f, 86.0f), 1);
		UTextBlock* Title = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryGroupTitle_%d"), Group.Index),
			22, FLinearColor(0.94f, 0.88f, 0.62f, 1.0f), true);
		Title->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Group.TitleLocalizationId)));
		PlaceOnCanvas(Header, Title, FVector2D(18.0f, 9.0f), FVector2D(570.0f, 32.0f), 2);
		UTextBlock* Summary = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryGroupSummary_%d"), Group.Index),
			15, FLinearColor(0.84f, 0.84f, 0.80f, 1.0f), false);
		Summary->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Group.HeaderLocalizationId)));
		Summary->SetAutoWrapText(true);
		PlaceOnCanvas(Header, Summary, FVector2D(18.0f, 45.0f), FVector2D(975.0f, 35.0f), 2);
		UTextBlock* Expand = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryExpand_%d"), Group.Index),
			26, FLinearColor(0.94f, 0.88f, 0.62f, 1.0f), true, ETextJustify::Center);
		Expand->SetText(FText::FromString(bExpanded ? TEXT("−") : TEXT("+")));
		PlaceOnCanvas(Header, Expand, FVector2D(1000.0f, 22.0f), FVector2D(44.0f, 42.0f), 3);
		PlaceOnCanvas(Header, MakeActionButton(*FString::Printf(TEXT("AllianceTerritoryGroupHit_%d"), Group.Index),
			19620 + Group.Index, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(1052.0f, 86.0f)),
			FVector2D::ZeroVector, FVector2D(1052.0f, 86.0f), 5);
		Y += 96.0f;
		if (!bExpanded) continue;
		const bool bResourceSummary = Group.Index == 3;
		const float RowHeight = bResourceSummary ? 290.0f : 370.0f;
		const float RowWidth = bResourceSummary ? 930.0f : 1030.0f;
		// Unity building row GridLayoutGroup: four 225x300 cells, 10 horizontal spacing.
		const float SlotWidth = 225.0f;
		UCanvasPanel* RowPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("AllianceTerritoryRow_%d"), Group.Index));
		PlaceOnCanvas(List, RowPanel, FVector2D(bResourceSummary ? 61.0f : 11.0f, Y),
			FVector2D(RowWidth, RowHeight), 1);
		PlaceOnCanvas(RowPanel, MakeSolidBorder(*FString::Printf(TEXT("AllianceTerritoryRowBg_%d"), Group.Index),
			FLinearColor(0.13f, 0.13f, 0.12f, 0.98f)), FVector2D::ZeroVector,
			FVector2D(RowWidth, bResourceSummary ? RowHeight : RowHeight - 10.0f), 1);
		if (bResourceSummary)
		{
			// Unity prefab_index=1 is a 930x290 summary row, not building cells.
			UTextBlock* ConfigText = MakeScreenText(TEXT("AllianceTerritoryResourceDefinitions"), 16,
				FLinearColor(0.86f, 0.84f, 0.78f, 1.0f), false);
			ConfigText->SetText(NSLOCTEXT("RokAllianceTerritory", "ResourceDefinitionReference", "静态资源配置定义 · 非实际占领资源点"));
			PlaceOnCanvas(RowPanel, ConfigText, FVector2D(12.0f, 8.0f), FVector2D(906.0f, 40.0f), 2);
			int32 ResourceSlotIndex = 0;
			for (const RokAllianceTerritoryReadonly::FRow& Definition : Rows)
			{
				if (Definition.GroupIndex != Group.Index) continue;
				const FVector2D ResourceSlotPos(ResourceSlotIndex * (SlotWidth + 10.0f), 55.0f);
				UTextBlock* ResourceName = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryResourceName_%d"), Definition.UnityType),
					17, FLinearColor(0.94f, 0.88f, 0.62f, 1.0f), true);
				ResourceName->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Definition.NameLocalizationId)));
				ResourceName->SetAutoWrapText(true);
				PlaceOnCanvas(RowPanel, ResourceName, ResourceSlotPos + FVector2D(9.0f, 12.0f), FVector2D(207.0f, 80.0f), 3);
				UTextBlock* ResourceIconState = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryResourceIconState_%d"), Definition.UnityType),
					14, FLinearColor(0.84f, 0.80f, 0.69f, 1.0f), false);
				ResourceIconState->SetText(Definition.bSourceIconMissing
					? NSLOCTEXT("RokAllianceTerritory", "ResourceMalformedSourceIcon", "源图标缺失")
					: NSLOCTEXT("RokAllianceTerritory", "ResourcePhysicalIconPending", "图标未接入"));
				PlaceOnCanvas(RowPanel, ResourceIconState, ResourceSlotPos + FVector2D(9.0f, 107.0f), FVector2D(207.0f, 35.0f), 3);
				PlaceOnCanvas(RowPanel, MakeActionButton(*FString::Printf(TEXT("AllianceTerritorySelect_%d"), Definition.UnityType),
					19600 + Definition.UnityType, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(SlotWidth, 182.0f)),
					ResourceSlotPos, FVector2D(SlotWidth, 182.0f), 5);
				++ResourceSlotIndex;
			}
			Y += RowHeight + 10.0f;
			continue;
		}
		int32 SlotIndex = 0;
		for (const RokAllianceTerritoryReadonly::FRow& Definition : Rows)
		{
			if (Definition.GroupIndex != Group.Index) continue;
			UCanvasPanel* DetailIconSlot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
				*FString::Printf(TEXT("AllianceTerritorySlot_%d"), Definition.UnityType));
			PlaceOnCanvas(RowPanel, DetailIconSlot, FVector2D(SlotIndex * (SlotWidth + 10.0f), 0.0f),
				FVector2D(SlotWidth, 300.0f), 2);
			PlaceOnCanvas(DetailIconSlot, MakeSolidBorder(*FString::Printf(TEXT("AllianceTerritorySlotBg_%d"), Definition.UnityType),
				SelectedAllianceTerritoryType == Definition.UnityType
				? FLinearColor(0.30f, 0.25f, 0.16f, 0.98f) : FLinearColor(0.19f, 0.19f, 0.17f, 0.98f)),
				FVector2D::ZeroVector, FVector2D(SlotWidth, 300.0f), 1);
			UTextBlock* Name = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryName_%d"), Definition.UnityType),
				18, FLinearColor(0.94f, 0.88f, 0.62f, 1.0f), true);
			Name->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Definition.NameLocalizationId)));
			Name->SetAutoWrapText(true);
			PlaceOnCanvas(DetailIconSlot, Name, FVector2D(10.0f, 12.0f), FVector2D(SlotWidth - 25.0f, 50.0f), 2);
			UTextBlock* Identity = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryType_%d"), Definition.UnityType),
				14, FLinearColor(0.76f, 0.80f, 0.76f, 1.0f), false);
			Identity->SetText(FText::FromString(FString::Printf(TEXT("配置 type %d"), Definition.UnityType)));
			PlaceOnCanvas(DetailIconSlot, Identity, FVector2D(10.0f, 70.0f), FVector2D(SlotWidth - 25.0f, 24.0f), 2);
			UTextBlock* IconState = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryIconState_%d"), Definition.UnityType),
				14, FLinearColor(0.79f, 0.74f, 0.65f, 1.0f), false);
			IconState->SetText(Definition.bSourceIconMissing
				? NSLOCTEXT("RokAllianceTerritory", "MalformedSourceIcon", "源图标缺失")
				: (Definition.SourceIconReference[0] == 0
					? NSLOCTEXT("RokAllianceTerritory", "NoSourceIcon", "源未配置图标")
					: NSLOCTEXT("RokAllianceTerritory", "PhysicalIconPending", "图标未接入")));
			PlaceOnCanvas(DetailIconSlot, IconState, FVector2D(10.0f, 102.0f), FVector2D(SlotWidth - 25.0f, 24.0f), 2);
			UTextBlock* Description = MakeScreenText(*FString::Printf(TEXT("AllianceTerritoryDescription_%d"), Definition.UnityType),
				14, FLinearColor(0.86f, 0.84f, 0.78f, 1.0f), false);
			Description->SetText(FText::FromString(RokAllianceTerritoryReadonly::GetChineseSourceText(Definition.DescriptionLocalizationId)));
			Description->SetAutoWrapText(true);
			PlaceOnCanvas(DetailIconSlot, Description, FVector2D(10.0f, 139.0f),
				FVector2D(SlotWidth - 25.0f, RowHeight - 173.0f), 2);
			PlaceOnCanvas(DetailIconSlot, MakeActionButton(*FString::Printf(TEXT("AllianceTerritorySelect_%d"), Definition.UnityType),
				19600 + Definition.UnityType, nullptr, FIntRect(), FIntPoint::ZeroValue,
				FVector2D(SlotWidth, 300.0f)),
				FVector2D::ZeroVector, FVector2D(SlotWidth, 300.0f), 5);
			++SlotIndex;
		}
		Y += RowHeight + 10.0f;
	}
	UTextBlock* Selection = MakeScreenText(TEXT("AllianceTerritoryReferenceSelection"), 15,
		FLinearColor(0.84f, 0.80f, 0.69f, 1.0f), false);
	Selection->SetText(SelectedAllianceTerritoryType > 0
		? FText::FromString(FString::Printf(TEXT("已选静态定义 type %d · 无联盟实际拥有记录"), SelectedAllianceTerritoryType))
		: NSLOCTEXT("RokAllianceTerritory", "NoSelection", "点击分组展开配置定义；实际领土记录未同步"));
	PlaceOnCanvas(ModeCanvas, Selection, CapturePoint(198.0f, 744.0f), FVector2D(840.0f, 42.0f), 6);
}

void URokUnityFeatureScreenWidget::BuildAllianceShopReference()
{
	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAllianceShop", "ReferenceTitle", "联盟商店配置参考"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceShopReferenceShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);
	UTextBlock* Boundary = MakeScreenText(TEXT("AllianceShopReferenceBoundary"), 16,
		FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
	Boundary->SetText(NSLOCTEXT("RokAllianceShop", "ReferenceBoundary",
		"仅显示商品配置定义和配置价；实际库存、积分余额、权限与购买状态均需服务器数据。"));
	PlaceOnCanvas(ModeCanvas, Boundary, CapturePoint(198.0f, 156.0f), FVector2D(1020.0f, 34.0f), 5);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceShopReferenceBack"), 690,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f),
		NSLOCTEXT("RokAllianceShop", "ReferenceBack", "返回"), 18,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(1090.0f, 738.0f), FVector2D(138.0f, 54.0f), 7);
	URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
	if (!Catalog || !Catalog->LoadAllianceShopReadonly())
	{
		UTextBlock* Missing = MakeScreenText(TEXT("AllianceShopReferenceUnavailable"), 20,
			FLinearColor(0.88f, 0.82f, 0.70f, 1.0f), false, ETextJustify::Center);
		Missing->SetText(NSLOCTEXT("RokAllianceShop", "ReferenceUnavailable",
			"联盟商店静态配置不可用；未读取库存或购买状态"));
		PlaceOnCanvas(ModeCanvas, Missing, CapturePoint(250.0f, 410.0f), FVector2D(900.0f, 50.0f), 6);
		return;
	}
	const TArray<FRokUnityAllianceShopReadonlyRow>& Rows = Catalog->GetAllianceShopReadonlyRows();
	const TArray<FRokUnityAllianceShopGroupReadonlyRow>& Groups = Catalog->GetAllianceShopReadonlyGroups();
	bool bSelectionInGroup = false;
	for (const FRokUnityAllianceShopReadonlyRow& Row : Rows)
	{
		if (Row.Group == SelectedAllianceShopGroup && Row.ItemType == SelectedAllianceShopItemType)
		{
			bSelectionInGroup = true;
			break;
		}
	}
	if (!bSelectionInGroup)
	{
		for (const FRokUnityAllianceShopReadonlyRow& Row : Rows)
		{
			if (Row.Group == SelectedAllianceShopGroup)
			{
				SelectedAllianceShopItemType = Row.ItemType;
				break;
			}
		}
	}
	for (const FRokUnityAllianceShopGroupReadonlyRow& Group : Groups)
	{
		const bool bSelected = SelectedAllianceShopGroup == Group.Group;
		PlaceOnCanvas(ModeCanvas, MakeActionButton(
			*FString::Printf(TEXT("AllianceShopGroup_%d"), Group.Group), 690 + Group.Group,
			CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(164.0f, 48.0f),
			FText::FromString(Group.Name), 18,
			bSelected ? FLinearColor(0.18f, 0.13f, 0.05f, 1.0f)
				: FLinearColor(0.04f, 0.14f, 0.24f, 1.0f),
			ESlateBrushDrawType::Box, MiniBlueMargin),
			CapturePoint(196.0f + (Group.Group - 1) * 173.0f, 197.0f),
			FVector2D(164.0f, 48.0f), 7);
	}
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceShopListPanel"),
		FLinearColor(0.15f, 0.15f, 0.14f, 0.98f)),
		CapturePoint(194.0f, 262.0f), FVector2D(636.0f, 482.0f), 3);
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceShopDetailPanel"),
		FLinearColor(0.20f, 0.19f, 0.17f, 0.98f)),
		CapturePoint(848.0f, 262.0f), FVector2D(385.0f, 458.0f), 3);
	const FRokUnityAllianceShopReadonlyRow* Selected = nullptr;
	int32 VisibleIndex = 0;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FRokUnityAllianceShopReadonlyRow& Row = Rows[Index];
		if (Row.Group != SelectedAllianceShopGroup) continue;
		if (Row.ItemType == SelectedAllianceShopItemType) Selected = &Row;
		const float Y = 270.0f + VisibleIndex * 52.0f;
		const bool bSelected = Row.ItemType == SelectedAllianceShopItemType;
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(
			*FString::Printf(TEXT("AllianceShopRow_%d"), Row.ItemType),
			bSelected ? FLinearColor(0.34f, 0.31f, 0.24f, 0.98f)
				: FLinearColor(0.19f, 0.18f, 0.16f, 0.98f)),
			CapturePoint(202.0f, Y), FVector2D(618.0f, 48.0f), 4);
		const TCHAR* IconPath = Row.IconAtlas == TEXT("item1") ? Item1AtlasPath
			: Row.IconAtlas == TEXT("item2") ? Item2AtlasPath
			: Row.IconAtlas == TEXT("item3") ? Item3AtlasPath : Item5AtlasPath;
		if (const UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, IconPath))
		{
			if (Texture->GetSizeX() == Row.AtlasSize.X && Texture->GetSizeY() == Row.AtlasSize.Y)
			{
				PlaceOnCanvas(ModeCanvas, MakeAtlasImage(
					*FString::Printf(TEXT("AllianceShopIcon_%d"), Row.ItemType),
					IconPath, Row.IconRect, Row.AtlasSize, FVector2D(40.0f)),
					CapturePoint(209.0f, Y + 4.0f), FVector2D(40.0f), 5);
			}
		}
		UTextBlock* Label = MakeScreenText(
			*FString::Printf(TEXT("AllianceShopName_%d"), Row.ItemType), 17,
			FLinearColor(0.91f, 0.85f, 0.67f, 1.0f), true);
		Label->SetText(FText::FromString(FString::Printf(TEXT("%s · ID %d · 品质 %d"),
			*Row.Name, Row.ItemType, Row.Quality)));
		PlaceOnCanvas(ModeCanvas, Label, CapturePoint(261.0f, Y + 9.0f),
			FVector2D(540.0f, 30.0f), 6);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(
			*FString::Printf(TEXT("AllianceShopSelect_%d"), Row.ItemType), 18100 + Index,
			nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(618.0f, 48.0f)),
			CapturePoint(202.0f, Y), FVector2D(618.0f, 48.0f), 8);
		++VisibleIndex;
	}
	if (!Selected) return;
	const TCHAR* DetailIconPath = Selected->IconAtlas == TEXT("item1") ? Item1AtlasPath
		: Selected->IconAtlas == TEXT("item2") ? Item2AtlasPath
		: Selected->IconAtlas == TEXT("item3") ? Item3AtlasPath : Item5AtlasPath;
	if (const UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, DetailIconPath))
	{
		if (Texture->GetSizeX() == Selected->AtlasSize.X && Texture->GetSizeY() == Selected->AtlasSize.Y)
		{
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceShopDetailQuality"),
				Item0AtlasPath, ResolveBagQualityRect(Selected->Quality), Item0AtlasSize,
				FVector2D(100.0f)), CapturePoint(990.0f, 282.0f), FVector2D(100.0f), 5);
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceShopDetailIcon"),
				DetailIconPath, Selected->IconRect, Selected->AtlasSize, FVector2D(88.0f)),
				CapturePoint(996.0f, 288.0f), FVector2D(88.0f), 6);
		}
	}
	UTextBlock* Name = MakeScreenText(TEXT("AllianceShopDetailName"), 20,
		FLinearColor(0.93f, 0.87f, 0.64f, 1.0f), true, ETextJustify::Center);
	Name->SetText(FText::FromString(Selected->Name));
	PlaceOnCanvas(ModeCanvas, Name, CapturePoint(870.0f, 403.0f), FVector2D(340.0f, 36.0f), 6);
	UTextBlock* Identity = MakeScreenText(TEXT("AllianceShopDetailIdentity"), 16,
		FLinearColor(0.82f, 0.82f, 0.77f, 1.0f), false);
	Identity->SetText(FText::FromString(FString::Printf(TEXT("物品 ID %d · 配置品质 %d"),
		Selected->ItemType, Selected->Quality)));
	PlaceOnCanvas(ModeCanvas, Identity, CapturePoint(876.0f, 452.0f), FVector2D(340.0f, 27.0f), 6);
	UTextBlock* BuyPrice = MakeScreenText(TEXT("AllianceShopDetailBuyConfigPrice"), 16,
		FLinearColor(0.85f, 0.83f, 0.76f, 1.0f), false);
	BuyPrice->SetText(FText::FromString(FString::Printf(
		TEXT("个人积分购买配置价：%d"), Selected->SellingPrice)));
	PlaceOnCanvas(ModeCanvas, BuyPrice, CapturePoint(876.0f, 500.0f), FVector2D(340.0f, 30.0f), 6);
	UTextBlock* StockPrice = MakeScreenText(TEXT("AllianceShopDetailStockConfigPrice"), 16,
		FLinearColor(0.85f, 0.83f, 0.76f, 1.0f), false);
	StockPrice->SetText(FText::FromString(FString::Printf(
		TEXT("联盟积分补货配置价：%d"), Selected->StockPrice)));
	PlaceOnCanvas(ModeCanvas, StockPrice, CapturePoint(876.0f, 540.0f), FVector2D(340.0f, 30.0f), 6);
	UTextBlock* State = MakeScreenText(TEXT("AllianceShopDetailServerBoundary"), 15,
		FLinearColor(0.77f, 0.81f, 0.71f, 1.0f), false);
	State->SetAutoWrapText(true);
	State->SetText(NSLOCTEXT("RokAllianceShop", "ServerBoundary",
		"此处不能查询实际库存、积分余额、可购买数量或购买结果。"));
	PlaceOnCanvas(ModeCanvas, State, CapturePoint(876.0f, 599.0f), FVector2D(340.0f, 74.0f), 6);
	UE_LOG(LogTemp, Display,
		TEXT("Rok alliance shop static definitions: group=%d item_type=%d rows=%d authority=local_reference state_unchanged=1."),
		SelectedAllianceShopGroup, Selected->ItemType, VisibleIndex);
}

void URokUnityFeatureScreenWidget::BuildAllianceGiftReference()
{
	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAllianceGift", "ReferenceTitle", "联盟礼物配置参考"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceGiftReferenceShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);
	UTextBlock* Boundary = MakeScreenText(TEXT("AllianceGiftReferenceBoundary"), 16,
		FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
	Boundary->SetText(NSLOCTEXT("RokAllianceGift", "ReferenceBoundary",
		"仅显示静态配置定义；当前联盟等级、积分、钥匙、礼物列表和领取状态均需服务器数据。"));
	PlaceOnCanvas(ModeCanvas, Boundary, CapturePoint(198.0f, 157.0f), FVector2D(1030.0f, 34.0f), 5);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceGiftReferenceBack"), 686,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f),
		NSLOCTEXT("RokAllianceGift", "ReferenceBack", "返回"), 18,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(837.0f, 198.0f), FVector2D(138.0f, 54.0f), 7);
	URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
	if (!Catalog || !Catalog->LoadAllianceGiftReadonly())
	{
		UTextBlock* Missing = MakeScreenText(TEXT("AllianceGiftReferenceUnavailable"), 20,
			FLinearColor(0.88f, 0.82f, 0.70f, 1.0f), false, ETextJustify::Center);
		Missing->SetText(NSLOCTEXT("RokAllianceGift", "ReferenceUnavailable",
			"礼物静态配置不可用；未生成礼物或领取状态"));
		PlaceOnCanvas(ModeCanvas, Missing, CapturePoint(250.0f, 410.0f), FVector2D(900.0f, 50.0f), 6);
		return;
	}
	const int32 Level = SelectedAllianceGiftConfiguredLevel;
	const FRokUnityAllianceGiftLevelReadonlyRow* LevelRow =
		Catalog->FindAllianceGiftLevelReadonly(Level);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceGiftReferencePrevious"), 687,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(115.0f, 48.0f),
		NSLOCTEXT("RokAllianceGift", "PreviousLevel", "上一级"), 17,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(198.0f, 200.0f), FVector2D(115.0f, 48.0f), 7);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceGiftReferenceNext"), 688,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(115.0f, 48.0f),
		NSLOCTEXT("RokAllianceGift", "NextLevel", "下一级"), 17,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(534.0f, 200.0f), FVector2D(115.0f, 48.0f), 7);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceGiftReferenceSpecial"), 689,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(180.0f, 48.0f),
		bAllianceGiftSpecialDefinitions
			? NSLOCTEXT("RokAllianceGift", "RegularDefinition", "常规等级定义")
			: NSLOCTEXT("RokAllianceGift", "SpecialDefinition", "特殊类型定义"), 17,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(1000.0f, 200.0f), FVector2D(180.0f, 48.0f), 7);
	UTextBlock* LevelLabel = MakeScreenText(TEXT("AllianceGiftReferenceLevel"), 21,
		FLinearColor(0.92f, 0.86f, 0.63f, 1.0f), true, ETextJustify::Center);
	LevelLabel->SetText(bAllianceGiftSpecialDefinitions
		? NSLOCTEXT("RokAllianceGift", "SpecialLevelLabel", "特殊类型 · 配置等级 0")
		: FText::FromString(FString::Printf(TEXT("配置等级 %d / 50"), Level)));
	PlaceOnCanvas(ModeCanvas, LevelLabel, CapturePoint(322.0f, 208.0f), FVector2D(205.0f, 34.0f), 6);
	UTextBlock* Threshold = MakeScreenText(TEXT("AllianceGiftReferenceThreshold"), 16,
		FLinearColor(0.78f, 0.82f, 0.75f, 1.0f), false);
	Threshold->SetText(!bAllianceGiftSpecialDefinitions && LevelRow
		? FText::FromString(FString::Printf(TEXT("该等级配置阈值 exp=%d · 宝箱配置 ID=%d"),
			LevelRow->ExpThreshold, LevelRow->TreasureId))
		: NSLOCTEXT("RokAllianceGift", "SpecialThreshold", "特殊类型无等级阈值；下列仅为静态定义"));
	PlaceOnCanvas(ModeCanvas, Threshold, CapturePoint(198.0f, 267.0f), FVector2D(1000.0f, 30.0f), 6);
	int32 RowIndex = 0;
	for (const FRokUnityAllianceGiftRewardReadonlyRow& Row : Catalog->GetAllianceGiftRewardReadonlyRows())
	{
		if (Row.GiftLevel != (bAllianceGiftSpecialDefinitions ? 0 : Level)) continue;
		const float Y = 306.0f + RowIndex * 51.0f;
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(
			*FString::Printf(TEXT("AllianceGiftReferenceRow_%d"), Row.Id),
			RowIndex % 2 == 0 ? FLinearColor(0.19f, 0.18f, 0.16f, 0.98f)
			: FLinearColor(0.24f, 0.22f, 0.19f, 0.98f)),
			CapturePoint(197.0f, Y), FVector2D(1020.0f, 45.0f), 4);
		UTextBlock* Detail = MakeScreenText(
			*FString::Printf(TEXT("AllianceGiftReferenceDefinition_%d"), Row.Id), 16,
			FLinearColor(0.89f, 0.87f, 0.81f, 1.0f), false);
		Detail->SetText(FText::FromString(FString::Printf(
			TEXT("类型 %d · 定义 ID %d · 奖励包 ID %d · 礼物积分 %d · 钥匙点数 %d"),
			Row.GiftType, Row.Id, Row.RewardPackageId, Row.GiftPoint, Row.KeyPoint)));
		PlaceOnCanvas(ModeCanvas, Detail, CapturePoint(218.0f, Y + 8.0f),
			FVector2D(980.0f, 30.0f), 6);
		++RowIndex;
	}
	UE_LOG(LogTemp, Display,
		TEXT("Rok alliance gift static definitions: configured_level=%d special=%d rows=%d authority=local_reference claim_state=0."),
		Level, bAllianceGiftSpecialDefinitions ? 1 : 0, RowIndex);
}

void URokUnityFeatureScreenWidget::BuildAllianceOfficeReference()
{
	BuildSplitWindowFrame(CapturePoint(170.0f, 73.0f), FVector2D(1100.0f, 754.0f),
		NSLOCTEXT("RokAlliance", "OfficeReferenceTitle", "官职配置参考"), CapturePoint(1230.0f, 77.0f));
	UTextBlock* Intro = MakeScreenText(TEXT("AllianceOfficeReferenceIntro"), 16,
		FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
	Intro->SetText(NSLOCTEXT("RokAlliance", "OfficeReferenceBoundary",
		"仅展示配置定义，不代表当前任命、人数、权限或已生效加成；选择不会改变账号状态。"));
	PlaceOnCanvas(ModeCanvas, Intro, CapturePoint(210.0f, 158.0f), FVector2D(1010.0f, 34.0f), 6);
	URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
	const bool bDefinitionsLoaded = Catalog && Catalog->LoadAllianceOfficeReadonly();
	const TArray<FRokUnityAllianceOfficeReadonlyRow>* OfficeRows = bDefinitionsLoaded
		? &Catalog->GetAllianceOfficeReadonlyRows() : nullptr;
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
			Name->SetText(Office.Name.IsEmpty()
				? FText::FromString(FString::Printf(TEXT("#%d 官职名称配置不可用"), Office.Id))
				: FText::FromString(FString::Printf(TEXT("#%d · %s"), Office.Id, *Office.Name)));
			PlaceOnCanvas(ModeCanvas, Name, CapturePoint(302.0f, Y + 5.0f), FVector2D(880.0f, 30.0f), 6);
			UTextBlock* Description = MakeScreenText(*FString::Printf(TEXT("AllianceOfficeReferenceDescription_%d"), Office.Id),
				16, FLinearColor(0.84f, 0.84f, 0.81f, 1.0f), false);
			Description->SetText(Office.Description.IsEmpty()
				? NSLOCTEXT("RokAlliance", "OfficeDescriptionUnavailable", "职位说明配置不可用")
				: FText::FromString(Office.Description));
			PlaceOnCanvas(ModeCanvas, Description, CapturePoint(302.0f, Y + 39.0f), FVector2D(870.0f, 26.0f), 6);
			UTextBlock* Bonus = MakeScreenText(*FString::Printf(TEXT("AllianceOfficeReferenceBonus_%d"), Office.Id),
				15, FLinearColor(0.75f, 0.82f, 0.66f, 1.0f), false);
			Bonus->SetAutoWrapText(true);
			Bonus->SetText(Office.AddDescription.IsEmpty()
				? NSLOCTEXT("RokAlliance", "OfficeBonusUnavailable", "配置加成说明不可用")
				: FText::FromString(FString::Printf(TEXT("配置加成说明：%s"), *Office.AddDescription)));
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

void URokUnityFeatureScreenWidget::BuildAllianceTechnologyReference()
{
	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAlliance", "TechnologyReferenceTitle", "联盟科技配置参考"),
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceTechnologyReferenceShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)),
		CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);
	UTextBlock* Boundary = MakeScreenText(TEXT("AllianceTechnologyReferenceBoundary"), 17,
		FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
	Boundary->SetText(NSLOCTEXT("RokAlliance", "TechnologyReferenceBoundary",
		"仅显示 AllianceStudy 静态配置；当前研究等级、进度、捐献与解锁由服务器决定"));
	PlaceOnCanvas(ModeCanvas, Boundary, CapturePoint(196.0f, 158.0f), FVector2D(1040.0f, 28.0f), 5);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceTechnologyReferenceBack"), 680,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f),
		NSLOCTEXT("RokAlliance", "TechnologyReferenceBack", "返回"), 18,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(1090.0f, 740.0f), FVector2D(138.0f, 54.0f), 7);
	URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
	if (!Catalog || !Catalog->LoadAllianceTechnologyReadonly())
	{
		UTextBlock* Missing = MakeScreenText(TEXT("AllianceTechnologyReferenceUnavailable"), 20,
			FLinearColor(0.88f, 0.82f, 0.70f, 1.0f), false, ETextJustify::Center);
		Missing->SetText(NSLOCTEXT("RokAlliance", "TechnologyReferenceUnavailable",
			"联盟科技静态配置不可用；未读取账户研究状态"));
		PlaceOnCanvas(ModeCanvas, Missing, CapturePoint(250.0f, 410.0f), FVector2D(900.0f, 50.0f), 6);
		return;
	}
	const TArray<FRokUnityAllianceTechnologyReadonlyRow>& Rows = Catalog->GetAllianceTechnologyReadonlyRows();
	static const FText CategoryLabels[] = {
		NSLOCTEXT("RokAlliance", "TechnologyDevelop", "发展"),
		NSLOCTEXT("RokAlliance", "TechnologyTerritory", "领土"),
		NSLOCTEXT("RokAlliance", "TechnologyWar", "战争"),
	};
	for (int32 Category = 1; Category <= 3; ++Category)
	{
		const bool bSelected = SelectedAllianceTechnologyCategory == Category;
		PlaceOnCanvas(ModeCanvas,
			MakeActionButton(*FString::Printf(TEXT("AllianceTechnologyCategory_%d"), Category),
				680 + Category, CommonAtlasPath, MiniBlueButtonRect,
				CommonAtlasSize, FVector2D(130.0f, 48.0f), CategoryLabels[Category - 1], 19,
				bSelected ? FLinearColor(0.18f, 0.13f, 0.05f, 1.0f)
				: FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box,
				MiniBlueMargin),
			CapturePoint(198.0f + (Category - 1) * 138.0f, 194.0f), FVector2D(130.0f, 48.0f), 7);
	}
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceTechnologyListPanel"),
		FLinearColor(0.15f, 0.15f, 0.14f, 0.98f)),
		CapturePoint(194.0f, 252.0f), FVector2D(414.0f, 474.0f), 3);
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceTechnologyDetailPanel"),
		FLinearColor(0.20f, 0.19f, 0.17f, 0.98f)),
		CapturePoint(622.0f, 194.0f), FVector2D(620.0f, 532.0f), 3);
	TArray<const FRokUnityAllianceTechnologyReadonlyRow*> Definitions;
	for (const FRokUnityAllianceTechnologyReadonlyRow& Row : Rows)
	{
		if (Row.CategoryType == SelectedAllianceTechnologyCategory && Row.LevelKey == 1)
		{
			Definitions.Add(&Row);
		}
	}
	if (Definitions.IsEmpty()) return;
	bool bSelectedTypeInCategory = false;
	for (const FRokUnityAllianceTechnologyReadonlyRow* Row : Definitions)
	{
		if (Row->StudyType == SelectedAllianceTechnologyStudyType) bSelectedTypeInCategory = true;
	}
	if (!bSelectedTypeInCategory)
	{
		SelectedAllianceTechnologyStudyType = Definitions[0]->StudyType;
		SelectedAllianceTechnologyLevelKey = 1;
	}
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),
		TEXT("AllianceTechnologyDefinitionScroll"));
	Scroll->SetOrientation(Orient_Vertical);
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	Scroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	PlaceOnCanvas(ModeCanvas, Scroll, CapturePoint(201.0f, 261.0f), FVector2D(400.0f, 457.0f), 4);
	USizeBox* Extent = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
		TEXT("AllianceTechnologyDefinitionExtent"));
	Extent->SetWidthOverride(400.0f);
	Extent->SetHeightOverride(Definitions.Num() * 70.0f);
	Scroll->AddChild(Extent);
	UCanvasPanel* List = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
		TEXT("AllianceTechnologyDefinitionList"));
	List->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Extent->AddChild(List);
	const TCHAR* Atlas1 = TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon1.rok_research_research_icon1");
	const TCHAR* Atlas2 = TEXT("/Game/RokPrototype/UI/ResearchAtlases/rok_research_research_icon2.rok_research_research_icon2");
	const UTexture2D* Icon1 = LoadObject<UTexture2D>(nullptr, Atlas1);
	const UTexture2D* Icon2 = LoadObject<UTexture2D>(nullptr, Atlas2);
	const bool bIcon1Available = Icon1 && Icon1->GetSizeX() == 512 && Icon1->GetSizeY() == 2048;
	const bool bIcon2Available = Icon2 && Icon2->GetSizeX() == 1024 && Icon2->GetSizeY() == 2048;
	for (int32 Index = 0; Index < Definitions.Num(); ++Index)
	{
		const FRokUnityAllianceTechnologyReadonlyRow& Row = *Definitions[Index];
		const bool bSelected = Row.StudyType == SelectedAllianceTechnologyStudyType;
		UCanvasPanel* Item = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("AllianceTechnologyDefinition_%d"), Row.StudyType));
		PlaceOnCanvas(List, Item, FVector2D(0.0f, Index * 70.0f), FVector2D(400.0f, 70.0f), 1);
		PlaceOnCanvas(Item, MakeSolidBorder(*FString::Printf(TEXT("AllianceTechnologyDefinitionBg_%d"), Row.StudyType),
			bSelected ? FLinearColor(0.34f, 0.31f, 0.24f, 0.98f)
			: FLinearColor(0.18f, 0.18f, 0.17f, 0.98f)),
			FVector2D(0.0f, 0.0f), FVector2D(392.0f, 66.0f), 1);
		const bool bIconAvailable = Row.IconAtlas == TEXT("research_icon1")
			? bIcon1Available : bIcon2Available;
		if (bIconAvailable)
		{
			PlaceOnCanvas(Item, MakeAtlasImage(*FString::Printf(TEXT("AllianceTechnologyIcon_%d"), Row.StudyType),
				Row.IconAtlas == TEXT("research_icon1") ? Atlas1 : Atlas2,
				Row.IconRect, Row.AtlasSize, FVector2D(52.0f)),
				FVector2D(7.0f, 7.0f), FVector2D(52.0f), 2);
		}
		UTextBlock* Label = MakeScreenText(*FString::Printf(TEXT("AllianceTechnologyName_%d"), Row.StudyType),
			18, FLinearColor(0.91f, 0.84f, 0.65f, 1.0f), true);
		Label->SetText(FText::FromString(Row.Title));
		PlaceOnCanvas(Item, Label, FVector2D(69.0f, 7.0f), FVector2D(300.0f, 27.0f), 3);
		UTextBlock* Identity = MakeScreenText(*FString::Printf(TEXT("AllianceTechnologyId_%d"), Row.StudyType),
			14, FLinearColor(0.80f, 0.80f, 0.76f, 1.0f), false);
		Identity->SetText(FText::FromString(FString::Printf(TEXT("studyType %d · 配置等级 1"), Row.StudyType)));
		PlaceOnCanvas(Item, Identity, FVector2D(69.0f, 38.0f), FVector2D(300.0f, 22.0f), 3);
		PlaceOnCanvas(Item, MakeActionButton(*FString::Printf(TEXT("AllianceTechnologyDefinitionHit_%d"), Row.StudyType),
			17000 + Row.StudyType, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(392.0f, 66.0f)),
			FVector2D::ZeroVector, FVector2D(392.0f, 66.0f), 5);
	}
	const FRokUnityAllianceTechnologyReadonlyRow* Selected = nullptr;
	for (const FRokUnityAllianceTechnologyReadonlyRow& Row : Rows)
	{
		if (Row.StudyType == SelectedAllianceTechnologyStudyType
			&& Row.LevelKey == SelectedAllianceTechnologyLevelKey)
		{
			Selected = &Row;
			break;
		}
	}
	if (!Selected) return;
	UTextBlock* Title = MakeScreenText(TEXT("AllianceTechnologyDetailTitle"), 27,
		FLinearColor(0.95f, 0.88f, 0.58f, 1.0f), true);
	Title->SetText(FText::FromString(Selected->Title));
	PlaceOnCanvas(ModeCanvas, Title, CapturePoint(754.0f, 216.0f), FVector2D(450.0f, 42.0f), 6);
	if (Selected->IconAtlas == TEXT("research_icon1") ? bIcon1Available : bIcon2Available)
	{
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceTechnologyDetailIcon"),
			Selected->IconAtlas == TEXT("research_icon1") ? Atlas1 : Atlas2,
			Selected->IconRect, Selected->AtlasSize, FVector2D(108.0f)),
			CapturePoint(640.0f, 209.0f), FVector2D(108.0f), 6);
	}
	else
	{
		UTextBlock* NoIcon = MakeScreenText(TEXT("AllianceTechnologyIconUnavailable"), 15,
			FLinearColor(0.84f, 0.76f, 0.63f, 1.0f), false, ETextJustify::Center);
		NoIcon->SetText(NSLOCTEXT("RokAlliance", "TechnologyIconUnavailable", "源图标资源不可用"));
		PlaceOnCanvas(ModeCanvas, NoIcon, CapturePoint(640.0f, 232.0f), FVector2D(108.0f, 52.0f), 6);
	}
	UTextBlock* Identity = MakeScreenText(TEXT("AllianceTechnologyDetailIdentity"), 18,
		FLinearColor(0.84f, 0.84f, 0.80f, 1.0f), false);
	Identity->SetText(FText::FromString(FString::Printf(TEXT("源配置行 ID %d · studyType %d · 配置等级 %d"),
		Selected->Id, Selected->StudyType, Selected->LevelKey)));
	PlaceOnCanvas(ModeCanvas, Identity, CapturePoint(648.0f, 333.0f), FVector2D(570.0f, 30.0f), 6);
	UTextBlock* Description = MakeScreenText(TEXT("AllianceTechnologyDetailDescription"), 19,
		FLinearColor(0.90f, 0.89f, 0.84f, 1.0f), false);
	Description->SetText(FText::FromString(Selected->Description));
	Description->SetAutoWrapText(true);
	PlaceOnCanvas(ModeCanvas, Description, CapturePoint(648.0f, 378.0f), FVector2D(565.0f, 114.0f), 6);
	FString Prerequisites = TEXT("配置前置 ID：");
	if (Selected->Prerequisites.IsEmpty()) Prerequisites += TEXT("无");
	for (int32 Index = 0; Index < Selected->Prerequisites.Num(); ++Index)
	{
		if (Index > 0) Prerequisites += TEXT("、");
		Prerequisites += FString::FromInt(Selected->Prerequisites[Index].SourceRowId);
	}
	UTextBlock* Precondition = MakeScreenText(TEXT("AllianceTechnologyDetailPrerequisites"), 17,
		FLinearColor(0.78f, 0.82f, 0.72f, 1.0f), false);
	Precondition->SetText(FText::FromString(Prerequisites));
	Precondition->SetAutoWrapText(true);
	PlaceOnCanvas(ModeCanvas, Precondition, CapturePoint(648.0f, 518.0f), FVector2D(565.0f, 60.0f), 6);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceTechnologyConfiguredPrevious"), 684,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 48.0f),
		NSLOCTEXT("RokAlliance", "TechnologyConfiguredPrevious", "上一配置级"), 17,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(648.0f, 646.0f), FVector2D(138.0f, 48.0f), 7);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceTechnologyConfiguredNext"), 685,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 48.0f),
		NSLOCTEXT("RokAlliance", "TechnologyConfiguredNext", "下一配置级"), 17,
		FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
		CapturePoint(806.0f, 646.0f), FVector2D(138.0f, 48.0f), 7);
}

void URokUnityFeatureScreenWidget::BuildAllianceCreate()
{
	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f), NSLOCTEXT("RokAlliance", "CreateAllianceTitle", "创建联盟"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceCreateShade"), FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)), CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);
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
			Input->SetHintText(Hint);
			FEditableTextBoxStyle InputStyle = FEditableTextBoxStyle::GetDefault();
			InputStyle
				.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 18))
				.SetForegroundColor(FLinearColor::White)
				.SetFocusedForegroundColor(FLinearColor::White)
				.SetBackgroundColor(FLinearColor(0.18f, 0.18f, 0.18f, 1.0f))
				.SetPadding(FMargin(10.0f, 6.0f));
			Input->SetWidgetStyle(InputStyle);
			Input->SetClearKeyboardFocusOnCommit(false);
			if (Index == 0) Input->SetText(FText::FromString(AllianceCreateNameDraft));
			else if (Index == 1) Input->SetText(FText::FromString(AllianceCreateTagDraft));
			else Input->SetText(FText::FromString(AllianceCreateNoticeDraft));
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
	const RokAllianceFlagLocalPreview::FEntry* Cloth = RokAllianceFlagLocalPreview::Find(1, AllianceCreateFlagDraft.ClothId);
	const RokAllianceFlagLocalPreview::FEntry* ClothColor = RokAllianceFlagLocalPreview::Find(2, AllianceCreateFlagDraft.ClothColorId);
	const RokAllianceFlagLocalPreview::FEntry* Emblem = RokAllianceFlagLocalPreview::Find(3, AllianceCreateFlagDraft.EmblemId);
	const RokAllianceFlagLocalPreview::FEntry* EmblemColor = RokAllianceFlagLocalPreview::Find(4, AllianceCreateFlagDraft.EmblemColorId);
	UImage* FlagCloth = MakeAtlasImage(TEXT("AllianceCreateFlagCloth"), AllianceFlagAtlasPath, Cloth ? Cloth->RealityRect : AllianceFlagClothRect, AllianceFlagAtlasSize, FVector2D(184.0f, 216.0f));
	FlagCloth->SetColorAndOpacity(ClothColor ? RokAllianceFlagLocalPreview::LinearColor(*ClothColor) : FLinearColor(0.14f, 0.36f, 0.72f, 1.0f));
	PlaceOnCanvas(ModeCanvas, FlagCloth, CapturePoint(925.0f, 258.0f), FVector2D(184.0f, 216.0f), 6);
	UImage* FlagEmblem = MakeAtlasImage(TEXT("AllianceCreateFlagEmblem"), AllianceFlagAtlasPath, Emblem ? Emblem->RealityRect : AllianceFlagEmblemRect, AllianceFlagAtlasSize, FVector2D(100.0f));
	FlagEmblem->SetColorAndOpacity(EmblemColor ? RokAllianceFlagLocalPreview::LinearColor(*EmblemColor) : FLinearColor(0.96f, 0.76f, 0.18f, 1.0f));
	PlaceOnCanvas(ModeCanvas, FlagEmblem, CapturePoint(967.0f, 310.0f), FVector2D(100.0f), 7);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceFlagEdit"), 625, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f), NSLOCTEXT("RokAlliance", "ChooseFlag", "选择旗帜"), 17, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(950.0f, 510.0f), FVector2D(138.0f, 54.0f), 8);

	PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceCreateGem"), CommonAtlasPath, GemRect, CommonAtlasSize, FVector2D(42.0f)), CapturePoint(854.0f, 644.0f), FVector2D(42.0f), 6);
	UTextBlock* Cost = MakeScreenText(TEXT("AllianceCreateCost"), 19, FLinearColor(0.96f, 0.88f, 0.55f, 1.0f), true);
	Cost->SetText(
		AllianceConfig.EstablishCost > 0
			? FText::Format(
				NSLOCTEXT("RokAlliance", "CreateCost", "创建消耗：{0}"),
				FText::AsNumber(AllianceConfig.EstablishCost))
			: NSLOCTEXT("RokAlliance", "CreateCostUnavailable", "创建消耗：--"));
	PlaceOnCanvas(ModeCanvas, Cost, CapturePoint(900.0f, 650.0f), FVector2D(210.0f, 32.0f), 6);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceCreateConfirm"), 623, CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize, FVector2D(202.0f, 81.0f), NSLOCTEXT("RokAlliance", "CreateNow", "创建"), 27, FLinearColor(0.23f, 0.16f, 0.04f, 1.0f)), CapturePoint(920.0f, 700.0f), FVector2D(202.0f, 81.0f), 8);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceCreateBack"), 622, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f), NSLOCTEXT("RokAlliance", "BackFromCreate", "返回"), 18, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(208.0f, 735.0f), FVector2D(138.0f, 54.0f), 8);
}

void URokUnityFeatureScreenWidget::BuildAllianceFlagEditor()
{
	using namespace RokAllianceFlagLocalPreview;
	BuildSplitWindowFrame(CapturePoint(146.0f, 73.0f), FVector2D(1148.0f, 754.0f),
		NSLOCTEXT("RokAllianceFlag", "LocalFlagTitle", "编辑联盟旗帜"), CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceFlagEditorShade"),
		FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)), CapturePoint(176.0f, 151.0f),
		FVector2D(1088.0f, 664.0f), 2);
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceFlagEditorPreviewPanel"),
		FLinearColor(0.23f, 0.22f, 0.19f, 1.0f)), CapturePoint(202.0f, 196.0f),
		FVector2D(300.0f, 454.0f), 4);
	UTextBlock* PreviewTitle = MakeScreenText(TEXT("AllianceFlagEditorPreviewTitle"), 22,
		FLinearColor(0.94f, 0.90f, 0.80f, 1.0f), true, ETextJustify::Center);
	PreviewTitle->SetText(NSLOCTEXT("RokAllianceFlag", "Preview", "旗帜预览"));
	PlaceOnCanvas(ModeCanvas, PreviewTitle, CapturePoint(220.0f, 212.0f), FVector2D(264.0f, 40.0f), 5);
	const FEntry* Cloth = Find(1, AllianceFlagEditDraft.ClothId);
	const FEntry* ClothColor = Find(2, AllianceFlagEditDraft.ClothColorId);
	const FEntry* Emblem = Find(3, AllianceFlagEditDraft.EmblemId);
	const FEntry* EmblemColor = Find(4, AllianceFlagEditDraft.EmblemColorId);
	if (Cloth && ClothColor && Emblem && EmblemColor)
	{
		UImage* ClothImage = MakeAtlasImage(TEXT("AllianceFlagEditorCloth"), AllianceFlagAtlasPath,
			Cloth->RealityRect, AllianceFlagAtlasSize, FVector2D(184.0f, 216.0f));
		ClothImage->SetColorAndOpacity(LinearColor(*ClothColor));
		PlaceOnCanvas(ModeCanvas, ClothImage, CapturePoint(260.0f, 276.0f), FVector2D(184.0f, 216.0f), 6);
		UImage* EmblemImage = MakeAtlasImage(TEXT("AllianceFlagEditorEmblem"), AllianceFlagAtlasPath,
			Emblem->RealityRect, AllianceFlagAtlasSize, FVector2D(100.0f));
		EmblemImage->SetColorAndOpacity(LinearColor(*EmblemColor));
		PlaceOnCanvas(ModeCanvas, EmblemImage, CapturePoint(302.0f, 328.0f), FVector2D(100.0f), 7);
	}
	UTextBlock* Identity = MakeScreenText(TEXT("AllianceFlagEditorSourceIds"), 15,
		FLinearColor(0.87f, 0.86f, 0.81f, 1.0f), false, ETextJustify::Center);
	Identity->SetText(FText::FromString(FString::Printf(TEXT("配置 ID  %d / %d / %d / %d"),
		AllianceFlagEditDraft.ClothId, AllianceFlagEditDraft.ClothColorId,
		AllianceFlagEditDraft.EmblemId, AllianceFlagEditDraft.EmblemColorId)));
	PlaceOnCanvas(ModeCanvas, Identity, CapturePoint(212.0f, 535.0f), FVector2D(280.0f, 32.0f), 8);
	UTextBlock* Local = MakeScreenText(TEXT("AllianceFlagEditorLocalOnly"), 16,
		FLinearColor(0.81f, 0.85f, 0.73f, 1.0f), false, ETextJustify::Center);
	Local->SetText(NSLOCTEXT("RokAllianceFlag", "LocalOnly", "仅编辑当前创建表单的本地预览"));
	PlaceOnCanvas(ModeCanvas, Local, CapturePoint(214.0f, 584.0f), FVector2D(278.0f, 48.0f), 8);

	static const FText TypeLabels[] = {
		NSLOCTEXT("RokAllianceFlag", "Cloth", "旗面图案"),
		NSLOCTEXT("RokAllianceFlag", "ClothColor", "旗面底色"),
		NSLOCTEXT("RokAllianceFlag", "Emblem", "联盟徽记"),
		NSLOCTEXT("RokAllianceFlag", "EmblemColor", "徽记颜色"),
	};
	for (int32 Type = 1; Type <= 4; ++Type)
	{
		const FVector2D TabPoint = CapturePoint(536.0f + 164.0f * (Type - 1), 203.0f);
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(*FString::Printf(TEXT("AllianceFlagTabPanel_%d"), Type),
			Type == SelectedAllianceFlagType ? FLinearColor(0.48f, 0.36f, 0.14f, 1.0f)
				: FLinearColor(0.22f, 0.21f, 0.20f, 1.0f)), TabPoint, FVector2D(154.0f, 54.0f), 5);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("AllianceFlagTab_%d"), Type),
			19000 + Type, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(154.0f, 54.0f),
			TypeLabels[Type - 1], 18, FLinearColor::White), TabPoint, FVector2D(154.0f, 54.0f), 8);
	}
	int32 ItemIndex = 0;
	for (const FEntry& Entry : Rows())
	{
		if (Entry.Type != SelectedAllianceFlagType) continue;
		const int32 Column = ItemIndex % 8;
		const int32 Row = ItemIndex / 8;
		const FVector2D ItemPoint = CapturePoint(541.0f + 82.0f * Column, 302.0f + 106.0f * Row);
		const bool bSelected = AllianceFlagEditDraft.GetId(Entry.Type) == Entry.Id;
		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(*FString::Printf(TEXT("AllianceFlagItemPanel_%d"), Entry.Id),
			bSelected ? FLinearColor(0.67f, 0.48f, 0.13f, 1.0f)
				: FLinearColor(0.20f, 0.20f, 0.19f, 1.0f)), ItemPoint, FVector2D(76.0f, 76.0f), 5);
		if (Entry.bHasColor)
		{
			PlaceOnCanvas(ModeCanvas, MakeSolidBorder(*FString::Printf(TEXT("AllianceFlagItemSwatch_%d"), Entry.Id),
				LinearColor(Entry)), ItemPoint + FVector2D(8.0f, 8.0f), FVector2D(60.0f, 60.0f), 6);
		}
		else
		{
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("AllianceFlagItemSprite_%d"), Entry.Id),
				AllianceFlagAtlasPath, Entry.ShowRect, AllianceFlagAtlasSize, FVector2D(64.0f, 64.0f)),
				ItemPoint + FVector2D(6.0f, 6.0f), FVector2D(64.0f, 64.0f), 6);
		}
		PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("AllianceFlagItemHit_%d"), Entry.Id),
			19000 + Entry.Type * 100 + ItemIndex, nullptr, FIntRect(), FIntPoint::ZeroValue,
			FVector2D(76.0f, 76.0f)), ItemPoint, FVector2D(76.0f, 76.0f), 8);
		UTextBlock* IdLabel = MakeScreenText(*FString::Printf(TEXT("AllianceFlagItemId_%d"), Entry.Id), 14,
			FLinearColor(0.86f, 0.83f, 0.73f, 1.0f), false, ETextJustify::Center);
		IdLabel->SetText(FText::AsNumber(Entry.Id));
		PlaceOnCanvas(ModeCanvas, IdLabel, ItemPoint + FVector2D(0.0f, 77.0f), FVector2D(76.0f, 24.0f), 8);
		++ItemIndex;
	}
	UTextBlock* Note = MakeScreenText(TEXT("AllianceFlagEditorBoundary"), 16,
		FLinearColor(0.82f, 0.82f, 0.77f, 1.0f), false, ETextJustify::Center);
	Note->SetText(NSLOCTEXT("RokAllianceFlag", "CreateBoundary", "确认仅更新本地预览；创建联盟仍需服务器确认。"));
	PlaceOnCanvas(ModeCanvas, Note, CapturePoint(531.0f, 662.0f), FVector2D(650.0f, 34.0f), 8);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceFlagEditorCancel"), 19010,
		CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(150.0f, 54.0f),
		NSLOCTEXT("RokAllianceFlag", "Cancel", "取消"), 19, FLinearColor::White,
		ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(822.0f, 727.0f), FVector2D(150.0f, 54.0f), 8);
	PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceFlagEditorConfirm"), 19011,
		CommonAtlasPath, MediumGoldButtonRect, CommonAtlasSize, FVector2D(160.0f, 59.0f),
		NSLOCTEXT("RokAllianceFlag", "Confirm", "确定"), 21, FLinearColor::White),
		CapturePoint(1000.0f, 724.0f), FVector2D(160.0f, 59.0f), 8);
}

void URokUnityFeatureScreenWidget::BuildAllianceJoined()
{
	static const FText PageLabels[] = {
		NSLOCTEXT("RokAlliance", "OverviewPage", "总览"),
		NSLOCTEXT("RokAlliance", "MemberPage", "成员"),
		NSLOCTEXT("RokAlliance", "SettingsPage", "设置"),
	};
	BuildSplitWindowFrame(
		CapturePoint(146.0f, 73.0f),
		FVector2D(1148.0f, 754.0f),
		SelectedAlliancePage == 1 ? NSLOCTEXT("RokAlliance", "MembersTitle", "联盟成员") : (SelectedAlliancePage == 2 ? NSLOCTEXT("RokAlliance", "SettingsTitle", "联盟设置") : NSLOCTEXT("RokAlliance", "JoinedTitle", "联盟")),
		CapturePoint(1230.0f, 77.0f));
	PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceJoinedShade"), FLinearColor(0.065f, 0.065f, 0.065f, 0.98f)), CapturePoint(176.0f, 151.0f), FVector2D(1088.0f, 664.0f), 2);

	for (int32 Page = 0; Page < 3; ++Page)
	{
		const bool bSelected = Page == SelectedAlliancePage;
		UCanvasPanel* Tab = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("AlliancePageTab_%d"), Page));
		PlaceOnCanvas(Tab, MakeAtlasImage(*FString::Printf(TEXT("AlliancePageTabBg_%d"), Page), CommonAtlasPath, bSelected ? SideTabSelectedRect : SideTabDarkRect, CommonAtlasSize, FVector2D(88.0f, 92.0f)), FVector2D::ZeroVector, FVector2D(88.0f, 92.0f), 0);
		PlaceOnCanvas(Tab, MakeAtlasImage(*FString::Printf(TEXT("AlliancePageIcon_%d"), Page), PageAtlasPath, AlliancePageIconRects[Page][bSelected ? 1 : 0], PageAtlasSize, FVector2D(48.0f)), FVector2D(20.0f, 10.0f), FVector2D(48.0f), 1);
		UTextBlock* PageText = MakeScreenText(*FString::Printf(TEXT("AlliancePageText_%d"), Page), 14, bSelected ? FLinearColor(0.93f, 0.94f, 0.91f, 1.0f) : FLinearColor(0.62f, 0.62f, 0.59f, 1.0f), true, ETextJustify::Center);
		PageText->SetText(PageLabels[Page]);
		PlaceOnCanvas(Tab, PageText, FVector2D(4.0f, 61.0f), FVector2D(80.0f, 24.0f), 2);
		PlaceOnCanvas(Tab, MakeActionButton(*FString::Printf(TEXT("AlliancePageHit_%d"), Page), 630 + Page, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(88.0f, 92.0f)), FVector2D::ZeroVector, FVector2D(88.0f, 92.0f), 5);
		PlaceOnCanvas(ModeCanvas, Tab, CapturePoint(1216.0f, 210.0f + Page * 112.0f), FVector2D(88.0f, 92.0f), 10);
	}

	// The joined preview only exists as a fixture: alliance membership is
	// server-authoritative, so the window must say it is not account state.
	UTextBlock* JoinedFixtureTag = MakeScreenText(TEXT("AllianceJoinedFixtureTag"), 15, FLinearColor(0.62f, 0.62f, 0.58f, 1.0f), false);
	JoinedFixtureTag->SetText(NSLOCTEXT("RokAlliance", "JoinedFixtureTag", "联盟 · 本地参考态（非账号状态）"));
	PlaceOnCanvas(ModeCanvas, JoinedFixtureTag, CapturePoint(195.0f, 748.0f), FVector2D(420.0f, 26.0f), 6);

	if (SelectedAlliancePage == 0)
	{
		PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("AllianceNoticePanel"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(555.0f, 145.0f), CommonPanelMargin), CapturePoint(195.0f, 175.0f), FVector2D(555.0f, 145.0f), 4);
		UTextBlock* NoticeTitle = MakeScreenText(TEXT("AllianceNoticeTitle"), 20, FLinearColor(0.95f, 0.88f, 0.58f, 1.0f), true);
		NoticeTitle->SetText(NSLOCTEXT("RokAlliance", "NoticeTitle", "联盟公告"));
		PlaceOnCanvas(ModeCanvas, NoticeTitle, CapturePoint(215.0f, 188.0f), FVector2D(180.0f, 32.0f), 6);
		UTextBlock* Notice = MakeScreenText(TEXT("AllianceJoinedNotice"), 18, FLinearColor(0.88f, 0.88f, 0.85f, 1.0f), false);
		Notice->SetText(NSLOCTEXT("RokAlliance", "JoinedNoticeText", "团结互助，共同建设联盟领土。欢迎每一位盟友！"));
		Notice->SetAutoWrapText(true);
		PlaceOnCanvas(ModeCanvas, Notice, CapturePoint(215.0f, 230.0f), FVector2D(510.0f, 68.0f), 6);

		static const int32 FeatureOrder[] = {3, 0, 6, 2, 7, 5, 1, 4};
		static const FText FeatureLabels[] = {
			NSLOCTEXT("RokAlliance", "AllianceWar", "联盟战争"),
			NSLOCTEXT("RokAlliance", "AllianceTerritory", "联盟领土"),
			NSLOCTEXT("RokAlliance", "HolyLand", "圣地"),
			NSLOCTEXT("RokAlliance", "AllianceHelpFeature", "联盟帮助"),
			NSLOCTEXT("RokAlliance", "AllianceDepot", "联盟仓库"),
			NSLOCTEXT("RokAlliance", "AllianceTechnology", "联盟科技"),
			NSLOCTEXT("RokAlliance", "AllianceGift", "联盟礼物"),
			NSLOCTEXT("RokAlliance", "AllianceStore", "联盟商店"),
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(FeatureLabels); ++Index)
		{
			const float X = 194.0f + (Index % 4) * 142.0f;
			const float Y = 338.0f + (Index / 4) * 178.0f;
			PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(*FString::Printf(TEXT("AllianceFeatureBg_%d"), Index), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(132.0f, 158.0f), CommonPanelMargin), CapturePoint(X, Y), FVector2D(132.0f, 158.0f), 4);
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("AllianceFeatureIcon_%d"), Index), AllianceAtlasPath, AllianceFeatureIconRects[FeatureOrder[Index]], AllianceAtlasSize, FVector2D(104.0f)), CapturePoint(X + 14.0f, Y + 10.0f), FVector2D(104.0f), 5);
			UTextBlock* Label = MakeScreenText(*FString::Printf(TEXT("AllianceFeatureLabel_%d"), Index), 16, FLinearColor(0.93f, 0.92f, 0.87f, 1.0f), true, ETextJustify::Center);
			Label->SetText(FeatureLabels[Index]);
			PlaceOnCanvas(ModeCanvas, Label, CapturePoint(X + 5.0f, Y + 121.0f), FVector2D(122.0f, 28.0f), 6);
			PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("AllianceFeatureHit_%d"), Index), 650 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(132.0f, 158.0f)), CapturePoint(X, Y), FVector2D(132.0f, 158.0f), 8);
		}

		PlaceOnCanvas(ModeCanvas, MakeSolidBorder(TEXT("AllianceSummaryPanel"), FLinearColor(0.23f, 0.22f, 0.19f, 0.98f)), CapturePoint(784.0f, 175.0f), FVector2D(420.0f, 570.0f), 4);
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceFlagBacking"), AllianceAtlasPath, AllianceFlagBackingRect, AllianceAtlasSize, FVector2D(170.0f, 208.0f)), CapturePoint(910.0f, 188.0f), FVector2D(170.0f, 208.0f), 5);
		UImage* JoinedFlagCloth = MakeAtlasImage(TEXT("AllianceJoinedFlagCloth"), AllianceFlagAtlasPath, AllianceFlagClothRect, AllianceFlagAtlasSize, FVector2D(126.0f, 148.0f));
		JoinedFlagCloth->SetColorAndOpacity(FLinearColor(0.14f, 0.36f, 0.72f, 1.0f));
		PlaceOnCanvas(ModeCanvas, JoinedFlagCloth, CapturePoint(932.0f, 215.0f), FVector2D(126.0f, 148.0f), 6);
		UImage* JoinedFlagEmblem = MakeAtlasImage(TEXT("AllianceJoinedFlagEmblem"), AllianceFlagAtlasPath, AllianceFlagEmblemRect, AllianceFlagAtlasSize, FVector2D(70.0f));
		JoinedFlagEmblem->SetColorAndOpacity(FLinearColor(0.96f, 0.76f, 0.18f, 1.0f));
		PlaceOnCanvas(ModeCanvas, JoinedFlagEmblem, CapturePoint(960.0f, 250.0f), FVector2D(70.0f), 7);

		UTextBlock* AllianceName = MakeScreenText(TEXT("AllianceJoinedName"), 25, FLinearColor(0.95f, 0.88f, 0.58f, 1.0f), true, ETextJustify::Center);
		AllianceName->SetText(NSLOCTEXT("RokAlliance", "JoinedAllianceName", "[SNOW] 雪原同盟"));
		PlaceOnCanvas(ModeCanvas, AllianceName, CapturePoint(810.0f, 405.0f), FVector2D(368.0f, 42.0f), 7);
		UTextBlock* Summary = MakeScreenText(TEXT("AllianceJoinedSummary"), 19, FLinearColor(0.90f, 0.90f, 0.86f, 1.0f), false);
		Summary->SetText(NSLOCTEXT("RokAlliance", "JoinedSummary", "战力：6,100\n盟主：snow\n领土：0\n礼物等级：1\n成员：1/50"));
		PlaceOnCanvas(ModeCanvas, Summary, CapturePoint(840.0f, 462.0f), FVector2D(310.0f, 160.0f), 7);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceMessageBoard"), 659, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f), NSLOCTEXT("RokAlliance", "MessageBoard", "留言板"), 17, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(825.0f, 664.0f), FVector2D(138.0f, 54.0f), 8);
		PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceMail"), 660, CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(138.0f, 54.0f), NSLOCTEXT("RokAlliance", "AllianceMail", "联盟邮件"), 17, FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin), CapturePoint(1015.0f, 664.0f), FVector2D(138.0f, 54.0f), 8);
	}
	else if (SelectedAlliancePage == 1)
	{
		PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(TEXT("AllianceLeaderCard"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(1040.0f, 145.0f), CommonPanelMargin), CapturePoint(198.0f, 175.0f), FVector2D(1040.0f, 145.0f), 4);
		PlaceOnCanvas(ModeCanvas, MakeAtlasImage(TEXT("AllianceLeaderRank"), AllianceAtlasPath, SpriteRect(898, 969, 100, 100), AllianceAtlasSize, FVector2D(92.0f)), CapturePoint(220.0f, 199.0f), FVector2D(92.0f), 5);
		UTextBlock* Leader = MakeScreenText(TEXT("AllianceLeaderName"), 24, FLinearColor(0.95f, 0.88f, 0.58f, 1.0f), true);
		Leader->SetText(NSLOCTEXT("RokAlliance", "LeaderPreview", "R5  snow（盟主）"));
		PlaceOnCanvas(ModeCanvas, Leader, CapturePoint(335.0f, 205.0f), FVector2D(360.0f, 38.0f), 6);
		UTextBlock* LeaderStats = MakeScreenText(TEXT("AllianceLeaderStats"), 18, FLinearColor(0.84f, 0.84f, 0.81f, 1.0f), false);
		LeaderStats->SetText(NSLOCTEXT("RokAlliance", "LeaderStatsPreview", "战力 6,100    击杀 0    在线"));
		PlaceOnCanvas(ModeCanvas, LeaderStats, CapturePoint(335.0f, 255.0f), FVector2D(430.0f, 30.0f), 6);
		URokUnityGameplayCatalogSubsystem* Catalog = GetGameplayCatalogSubsystem();
		const bool bDefinitionsLoaded = Catalog && Catalog->LoadAllianceOfficeReadonly();
		const TArray<FRokUnityAllianceOfficeReadonlyRow>* OfficeRows = bDefinitionsLoaded
			? &Catalog->GetAllianceOfficeReadonlyRows() : nullptr;
		UTextBlock* DefinitionNote = MakeScreenText(TEXT("AllianceOfficeReferenceNote"), 16,
			FLinearColor(0.89f, 0.85f, 0.67f, 1.0f), true);
		DefinitionNote->SetText(NSLOCTEXT("RokAlliance", "OfficeDefinitionBoundary",
			"官职配置参考 · 不代表当前任命、人数、权限或已生效加成"));
		PlaceOnCanvas(ModeCanvas, DefinitionNote, CapturePoint(212.0f, 318.0f),
			FVector2D(1000.0f, 24.0f), 6);
		if (OfficeRows && OfficeRows->Num() == 4)
		{
			for (int32 Index = 0; Index < OfficeRows->Num(); ++Index)
			{
				const FRokUnityAllianceOfficeReadonlyRow& Office = (*OfficeRows)[Index];
				const float Y = 348.0f + Index * 91.0f;
				const bool bSelected = Index == SelectedAllianceOfficeIndex;
				PlaceOnCanvas(ModeCanvas,
					MakeSolidBorder(*FString::Printf(TEXT("AllianceOfficeRow_%d"), Office.Id),
						bSelected ? FLinearColor(0.34f, 0.31f, 0.24f, 0.98f)
						: FLinearColor(0.16f, 0.16f, 0.16f, 0.96f)),
					CapturePoint(198.0f, Y), FVector2D(1040.0f, 78.0f), 4);
				const FIntRect IconRect = AllianceOfficeIconRect(Office.IconSprite);
				if (IconRect.Width() > 0 && IconRect.Height() > 0)
				{
					PlaceOnCanvas(ModeCanvas,
						MakeAtlasImage(*FString::Printf(TEXT("AllianceOfficeIcon_%d"), Office.Id),
							AllianceAtlasPath, IconRect, AllianceAtlasSize, FVector2D(58.0f)),
						CapturePoint(210.0f, Y + 10.0f), FVector2D(58.0f), 5);
				}
				UTextBlock* Name = MakeScreenText(*FString::Printf(TEXT("AllianceOfficeName_%d"), Office.Id),
					20, FLinearColor(0.91f, 0.84f, 0.65f, 1.0f), true);
				Name->SetText(Office.Name.IsEmpty()
					? NSLOCTEXT("RokAlliance", "OfficeNameUnavailable", "官职名称配置不可用")
					: FText::FromString(Office.Name));
				PlaceOnCanvas(ModeCanvas, Name, CapturePoint(282.0f, Y + 5.0f),
					FVector2D(880.0f, 28.0f), 6);
				UTextBlock* Description = MakeScreenText(
					*FString::Printf(TEXT("AllianceOfficeDescription_%d"), Office.Id), 15,
					FLinearColor(0.84f, 0.84f, 0.81f, 1.0f), false);
				Description->SetText(Office.Description.IsEmpty()
					? NSLOCTEXT("RokAlliance", "OfficeDescriptionUnavailable", "职位说明配置不可用")
					: FText::FromString(Office.Description));
				PlaceOnCanvas(ModeCanvas, Description, CapturePoint(282.0f, Y + 37.0f),
					FVector2D(445.0f, 28.0f), 6);
				UTextBlock* Bonus = MakeScreenText(
					*FString::Printf(TEXT("AllianceOfficeBonus_%d"), Office.Id), 14,
					FLinearColor(0.75f, 0.82f, 0.66f, 1.0f), false);
				Bonus->SetAutoWrapText(true);
				Bonus->SetText(Office.AddDescription.IsEmpty()
					? NSLOCTEXT("RokAlliance", "OfficeBonusUnavailable", "配置加成说明不可用")
					: FText::FromString(FString::Printf(TEXT("配置加成说明：%s"), *Office.AddDescription)));
				PlaceOnCanvas(ModeCanvas, Bonus, CapturePoint(740.0f, Y + 31.0f),
					FVector2D(474.0f, 42.0f), 6);
				PlaceOnCanvas(ModeCanvas, MakeActionButton(
					*FString::Printf(TEXT("AllianceOfficeDefinitionHit_%d"), Office.Id), 640 + Index,
					nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(1040.0f, 78.0f)),
					CapturePoint(198.0f, Y), FVector2D(1040.0f, 78.0f), 8);
			}
		}
		else
		{
			UTextBlock* Unavailable = MakeScreenText(TEXT("AllianceOfficeDefinitionsUnavailable"), 18,
				FLinearColor(0.82f, 0.80f, 0.76f, 1.0f), false, ETextJustify::Center);
			Unavailable->SetText(NSLOCTEXT("RokAlliance", "OfficeDefinitionsUnavailable",
				"官职静态配置暂不可用；未读取任命或权限状态"));
			PlaceOnCanvas(ModeCanvas, Unavailable, CapturePoint(250.0f, 440.0f),
				FVector2D(900.0f, 38.0f), 6);
		}
		PlaceOnCanvas(ModeCanvas, MakeActionButton(TEXT("AllianceMemberOfficeReferenceOpen"), 635,
			CommonAtlasPath, MiniBlueButtonRect, CommonAtlasSize, FVector2D(214.0f, 46.0f),
			NSLOCTEXT("RokAlliance", "MemberOfficeReferenceOpen", "官职定义说明"), 17,
			FLinearColor(0.04f, 0.14f, 0.24f, 1.0f), ESlateBrushDrawType::Box, MiniBlueMargin),
			CapturePoint(1095.0f, 735.0f), FVector2D(214.0f, 46.0f), 7);
	}
	else
	{
		static const FText SettingsLabels[] = {
			NSLOCTEXT("RokAlliance", "AllianceSettings", "联盟设置"),
			NSLOCTEXT("RokAlliance", "AllianceList", "联盟列表"),
			NSLOCTEXT("RokAlliance", "AllianceRank", "联盟排行"),
			NSLOCTEXT("RokAlliance", "ApplicationReview", "申请审核"),
			NSLOCTEXT("RokAlliance", "InviteMember", "邀请加入"),
			NSLOCTEXT("RokAlliance", "LeaveAlliance", "退出联盟"),
		};
		static const FIntRect SettingsIcons[] = {
			SpriteRect(163, 952, 145, 145),
			SpriteRect(1, 49, 145, 145),
			SpriteRect(1, 196, 145, 145),
			SpriteRect(1, 343, 145, 145),
			SpriteRect(1, 490, 145, 145),
			SpriteRect(1, 637, 145, 145),
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(SettingsLabels); ++Index)
		{
			const float X = 250.0f + (Index % 3) * 305.0f;
			const float Y = 205.0f + (Index / 3) * 275.0f;
			PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(*FString::Printf(TEXT("AllianceSettingBg_%d"), Index), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(230.0f, 220.0f), CommonPanelMargin), CapturePoint(X, Y), FVector2D(230.0f, 220.0f), 4);
			PlaceOnCanvas(ModeCanvas, MakeAtlasImage(*FString::Printf(TEXT("AllianceSettingIcon_%d"), Index), AllianceAtlasPath, SettingsIcons[Index], AllianceAtlasSize, FVector2D(128.0f)), CapturePoint(X + 51.0f, Y + 20.0f), FVector2D(128.0f), 5);
			UTextBlock* Setting = MakeScreenText(*FString::Printf(TEXT("AllianceSettingLabel_%d"), Index), 20, FLinearColor(0.92f, 0.90f, 0.84f, 1.0f), true, ETextJustify::Center);
			Setting->SetText(SettingsLabels[Index]);
			PlaceOnCanvas(ModeCanvas, Setting, CapturePoint(X + 10.0f, Y + 168.0f), FVector2D(210.0f, 36.0f), 6);
			PlaceOnCanvas(ModeCanvas, MakeActionButton(*FString::Printf(TEXT("AllianceSettingHit_%d"), Index), 670 + Index, nullptr, FIntRect(), FIntPoint::ZeroValue, FVector2D(230.0f, 220.0f)), CapturePoint(X, Y), FVector2D(230.0f, 220.0f), 8);
		}
	}
}

UCanvasPanel* URokUnityFeatureScreenWidget::MakeCommanderHead(
	int32 Index,
	const FIntRect& HeadRect,
	bool bRecruited,
	bool bSelected,
	const FText& DisplayName,
	const FText& StateText,
	int32 Rare)
{
	UCanvasPanel* Head = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *FString::Printf(TEXT("CommanderHead_%d"), Index));
	Head->SetClipping(EWidgetClipping::ClipToBounds);
	const bool bUsesHeroHeadFrame = Rare == 4 || Rare == 5;
	UImage* HeadFrame = MakeAtlasImage(
		*FString::Printf(TEXT("CommanderHeadFrame_%d"), Index),
		bUsesHeroHeadFrame ? CommanderHeadAtlasPath : CommanderUiAtlasPath,
		Rare == 5
			? CommanderLegendaryFrameRect
			: (Rare == 4 ? CommanderEpicFrameRect : CommanderRareFrameRect),
		bUsesHeroHeadFrame ? CommanderHeadAtlasSize : CommanderUiAtlasSize,
		FVector2D(116.0f, 132.0f));
	if (Rare == 3)
	{
		HeadFrame->SetColorAndOpacity(
			FLinearColor(0.55f, 0.78f, 1.0f, 1.0f));
	}
	else if (Rare == 2)
	{
		HeadFrame->SetColorAndOpacity(
			FLinearColor(0.62f, 1.0f, 0.70f, 1.0f));
	}
	PlaceOnCanvas(
		Head,
		HeadFrame,
		FVector2D(6.0f, 0.0f),
		FVector2D(116.0f, 132.0f),
		1);
	if (HeadRect.Width() > 0 && HeadRect.Height() > 0)
	{
		PlaceOnCanvas(Head, MakeAtlasImage(*FString::Printf(TEXT("CommanderHeadPortrait_%d"), Index), CommanderHeadAtlasPath, HeadRect, CommanderHeadAtlasSize, FVector2D(108.0f, 122.0f)), FVector2D(10.0f, 0.0f), FVector2D(108.0f, 122.0f), 2);
	}
	else
	{
		PlaceOnCanvas(
			Head,
			MakeSolidBorder(
				*FString::Printf(TEXT("CommanderHeadMissing_%d"), Index),
				FLinearColor(0.07f, 0.07f, 0.075f, 0.96f)),
			FVector2D(10.0f, 0.0f),
			FVector2D(108.0f, 122.0f),
			2);
	}
	if (!bRecruited)
	{
		PlaceOnCanvas(Head, MakeSolidBorder(*FString::Printf(TEXT("CommanderHeadDark_%d"), Index), FLinearColor(0.0f, 0.0f, 0.0f, 0.58f)), FVector2D(10.0f, 0.0f), FVector2D(108.0f, 122.0f), 3);
	}
	if (bSelected)
	{
		UBorder* Selection = MakeSolidBorder(*FString::Printf(TEXT("CommanderHeadSelection_%d"), Index), FLinearColor(1.0f, 0.75f, 0.06f, 0.28f));
		PlaceOnCanvas(Head, Selection, FVector2D(2.0f, 0.0f), FVector2D(124.0f, 136.0f), 5);
	}
	PlaceOnCanvas(
		Head,
		MakeSolidBorder(
			*FString::Printf(TEXT("CommanderHeadStateShade_%d"), Index),
			FLinearColor(0.0f, 0.0f, 0.0f, 0.72f)),
		FVector2D(10.0f, 109.0f),
		FVector2D(108.0f, 27.0f),
		6);
	UTextBlock* State = MakeScreenText(*FString::Printf(TEXT("CommanderHeadState_%d"), Index), 16, FLinearColor::White, true, ETextJustify::Center);
	State->SetText(StateText);
	PlaceOnCanvas(Head, State, FVector2D(14.0f, 112.0f), FVector2D(100.0f, 22.0f), 7);
	UButton* HeadAction = MakeActionButton(
		*FString::Printf(TEXT("CommanderHeadAction_%d"), Index),
		400 + Index,
		nullptr,
		FIntRect(),
		FIntPoint::ZeroValue,
		FVector2D(128.0f, 142.0f));
	HeadAction->SetToolTipText(FText::Format(
		bRecruited
			? NSLOCTEXT(
				"RokCommander",
				"OwnedCommanderTooltip",
				"{0} · 已拥有")
			: NSLOCTEXT(
				"RokCommander",
				"UnownedCommanderTooltip",
				"{0} · 未招募"),
		DisplayName));
	PlaceOnCanvas(
		Head,
		HeadAction,
		FVector2D::ZeroVector,
		FVector2D(128.0f, 142.0f),
		9);
	return Head;
}

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
	FeedbackText = MakeScreenText(TEXT("FeatureBoundaryFeedback"), 16, FLinearColor(1.0f, 0.87f, 0.45f, 1.0f), true, ETextJustify::Center);
	FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	FVector2D FeedbackPosition(360.0f, 778.0f);
	FVector2D FeedbackSize(720.0f, 28.0f);
	if (ActiveMode == ERokUnityFeatureScreenMode::Chat)
	{
		FeedbackPosition = FVector2D(350.0f, bChatEmojiPanelOpen ? 432.0f : 692.0f);
		FeedbackSize = FVector2D(645.0f, 28.0f);
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::WorldPositionSearch)
	{
		// The source uses TipStyle.Middle for its empty-input message.
		FeedbackPosition = FVector2D(560.0f, 385.0f);
		FeedbackSize = FVector2D(320.0f, 40.0f);
	}
	else if (ActiveMode == ERokUnityFeatureScreenMode::ResourceSearch)
	{
		const float PopupCenterX = 1160.0f
			- static_cast<float>(FMath::Clamp(SelectedResourceSearchType, 0, 4)) * 220.0f;
		FeedbackPosition = FVector2D(PopupCenterX - 174.84f, 507.92f);
		FeedbackSize = FVector2D(349.68f, 40.76f);
	}
	PlaceOnCanvas(ChatContentCanvas ? ChatContentCanvas.Get() : ModeCanvas.Get(), FeedbackText, FeedbackPosition, FeedbackSize, 50);
	if (ActiveMode == ERokUnityFeatureScreenMode::WorldPositionSearch)
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
	ActivityPreviewSecondsRemaining = FMath::Max(0.0, ActivityPreviewSecondsRemaining - static_cast<double>(DeltaSeconds));
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
	if (ActionId >= 320 && ActionId < 330)
	{
		const int32 RowIndex = ActionId - 320;
		if (VisibleTaskStates.IsValidIndex(RowIndex))
		{
			const FRokTaskRowState& RowState = VisibleTaskStates[RowIndex];
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok task action: task=%d page=%s state=%s authority=%s intent=%s state_unchanged=1."),
				RowState.TaskId,
				TaskRowPageToken(RowState.Page),
				TaskRowStateToken(RowState.State),
				*RowState.SourceAuthority.ToString(),
				TaskRowIntentToken(RowState.Intent));
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
	if (ActionId >= 400
		&& ActionId < 400 + VisibleCommanderIds.Num())
	{
		const int32 RequestedIndex = ActionId - 400;
		if (VisibleCommanderIds.IsValidIndex(RequestedIndex))
		{
			// Unity OnHeroSelected does nothing for the already selected hero.
			if (VisibleCommanderIds[RequestedIndex] == SelectedCommanderId) return;
			const int32 PreviousIndex = VisibleCommanderIds.IndexOfByKey(SelectedCommanderId);
			SelectedCommanderId = VisibleCommanderIds[RequestedIndex];
			SelectedCommanderSkillIndex = 0;
			SelectedCommanderSkillDetailIndex = INDEX_NONE;
			SelectedCommanderTalentIndex = INDEX_NONE;
			bCommanderSortMenuOpen = false;
			RebuildModeContent();
			if (CommanderAnimatedPortrait)
				CommanderAnimatedPortrait->BeginSelectionTransition(RequestedIndex > PreviousIndex);
			URokUnityGameplayCatalogSubsystem* GameplayCatalog =
				GetGameplayCatalogSubsystem();
			FRokUnityCatalogHero Hero;
			if (GameplayCatalog
				&& GameplayCatalog->GetHero(
					SelectedCommanderId,
					Hero))
			{
				UE_LOG(
					LogTemp,
					Log,
					TEXT("Rok commander select: hero=%d state_unchanged=1 authority=runtime_preview."),
					SelectedCommanderId);
				ShowBoundaryFeedback(FText::Format(
					NSLOCTEXT(
						"RokCommander",
						"CommanderSelected",
						"已选择统帅：{0}。"),
					FText::FromString(!Hero.DisplayNameZhCn.IsEmpty()
						? Hero.DisplayNameZhCn
						: Hero.DisplayNameEn)));
			}
		}
		return;
	}
	if (ActionId == 450)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Rok commander skill upgrade boundary: hero=%d state_unchanged=1 authority=server."),
			SelectedCommanderId);
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokCommander",
			"SkillUpgradeUnavailable",
			"技能升级功能暂未开放。"));
		return;
	}
	if (ActionId == 451)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Rok commander talent boundary: hero=%d state_unchanged=1 authority=server."),
			SelectedCommanderId);
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokCommander",
			"TalentUnavailable",
			"天赋功能暂未开放。"));
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
		URokUnityGameplayCatalogSubsystem* GameplayCatalog = GetGameplayCatalogSubsystem();
		FRokUnityCatalogHeroTalent Talent;
		const int32 TalentId = VisibleCommanderTalentIds.IsValidIndex(RequestedIndex)
			? VisibleCommanderTalentIds[RequestedIndex] : INDEX_NONE;
		if (!bExplicitBootstrapHero || !bInLocalPreview || !GameplayCatalog
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
	if (ActionId == 452)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("Rok commander summon boundary: hero=%d state_unchanged=1 authority=server."),
			SelectedCommanderId);
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokCommander",
			"SummonUnavailable",
			"招募功能暂未开放。"));
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
		URokUnityGameplayCatalogSubsystem* GameplayCatalog = GetGameplayCatalogSubsystem();
		FRokUnityCatalogHeroSkill Skill;
		FRokCommanderSkillReadonlyDetail Detail;
		const int32 SkillId = VisibleCommanderSkillIds.IsValidIndex(RequestedIndex)
			? VisibleCommanderSkillIds[RequestedIndex] : INDEX_NONE;
		if (!bExplicitBootstrapHero || !bInLocalPreview || !GameplayCatalog
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
			SelectedItemsUseCount = 1;
			RebuildModeContent();
		}
		return;
	}
	if (ActionId >= 520 && ActionId < 525)
	{
		SelectedItemsCategory = ActionId - 520;
		SelectedItemsIndex = 0;
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
	if (ActionId == 740)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok VIP boundary: action=daily_point_chest state_unchanged=1 authority=server."));
		ShowBoundaryFeedback(NSLOCTEXT("RokVip", "DailyPointBoundary", "每日VIP点数领取需要服务器确认，当前领取状态未改变。"));
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
	if (ActionId == 743)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok VIP boundary: action=free_chest state_unchanged=1 authority=server."));
		ShowBoundaryFeedback(NSLOCTEXT("RokVip", "FreeChestBoundary", "免费宝箱领取需要服务器状态，当前未发放奖励或隐藏红点。"));
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
	if (ActionId >= 1000 && ActionId < 1005)
	{
		SelectedResourceSearchType = ActionId - 1000;
		RebuildModeContent();
		return;
	}
	if (ActionId == 1010)
	{
		int32& SelectedLevel = SelectedResourceSearchType == 0
			? ResourceSearchBarbarianLevel
			: ResourceSearchResourceLevel;
		SelectedLevel = FMath::Max(1, SelectedLevel - 1);
		RebuildModeContent();
		return;
	}
	if (ActionId == 1011)
	{
		int32& SelectedLevel = SelectedResourceSearchType == 0
			? ResourceSearchBarbarianLevel
			: ResourceSearchResourceLevel;
		const int32 MaxLevel = SelectedResourceSearchType == 0 ? 25 : 6;
		SelectedLevel = FMath::Min(MaxLevel, SelectedLevel + 1);
		RebuildModeContent();
		return;
	}
	if (ActionId == 1012)
	{
		ShowBoundaryFeedback(
			NSLOCTEXT(
				"RokResourceSearch",
				"SearchBoundary",
				"搜索服务尚未接入，未移动镜头。"));
		return;
	}
	if (ActionId == 1013
		&& ActiveMode == ERokUnityFeatureScreenMode::ResourceSearch)
	{
		RequestCloseScreen();
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Expedition
		&& ActionId >= 24001 && ActionId <= 24080)
	{
		if (bExpeditionTaskConfigVisible) return;
		const int32 StageId = ActionId - 24000;
		FRokExpeditionStagePreviewRow Row;
		if (RokExpeditionStagePreviewData::FindById(ExpeditionRows, StageId, Row))
		{
			SelectedExpeditionStageId = StageId;
			RebuildModeContent();
		}
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Expedition && ActionId == 1123)
	{
		if (!bExpeditionTaskConfigVisible && ExpeditionRows.Num() == 80)
		{
			bExpeditionTaskConfigVisible = true;
			bExpeditionPreview = false;
			RebuildModeContent();
		}
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Expedition && ActionId == 1121)
	{
		if (bExpeditionTaskConfigVisible && !bExpeditionPreview && ExpeditionRows.Num() == 80)
		{
			bExpeditionPreview = true;
			ExpeditionAnimationPhase = FName(TEXT("PreviewTransition"));
			ExpeditionAnimationElapsed = 0;
			AdvanceExpeditionAnimation(0);
		}
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Expedition && ActionId == 1122)
	{
		if (!bExpeditionTaskConfigVisible) return;
		if (bExpeditionPreview)
		{
			bExpeditionPreview = false;
			ExpeditionAnimationPhase = FName(TEXT("NormalTransition"));
			ExpeditionAnimationElapsed = 0;
			AdvanceExpeditionAnimation(0);
		}
		else
		{
			// The source PrepareNormal path asks to exit its battle scene.
			// This local config viewer never entered that scene, so Back only
			// returns to the selected stage on the source map UI.
			bExpeditionTaskConfigVisible = false;
			RebuildModeContent();
		}
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Expedition && ActionId == 1120)
	{
		if (bExpeditionTaskConfigVisible && bExpeditionPreview) return;
		ExpeditionLastBoundaryActionId = ActionId;
		ShowBoundaryFeedback(
			NSLOCTEXT(
				"RokExpedition",
				"ChallengeBoundary",
				"远征编队、战斗结算与奖励服务尚未接入，当前没有消耗行动力或生成战斗结果。"));
		return;
	}
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
		SelectedBuildCityCategory = ActionId - 1300;
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
		const bool bPlacementTarget = TypeId == 2 || TypeId == 3 || TypeId == 13;
		FRokLocalConstructionPreview ConstructionPreview;
		bool bSourceEntryEvaluated = false;
		if (bPlacementTarget)
		{
			if (URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetWorld()
				? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr)
			{
				// The card has no tile yet; the city controller validates its
				// eventual position through PreviewLocalConstruction.
				bSourceEntryEvaluated = UpgradeSubsystem->PreviewLocalConstructionEntry(
					TypeId, ConstructionPreview);
			}
		}
		if (!BuildableBuildCityTypeIds.Contains(TypeId)
			|| (bPlacementTarget
				&& (!bSourceEntryEvaluated || !ConstructionPreview.bSourceEligible)))
		{
			if (bPlacementTarget && !ConstructionPreview.FailureReason.IsEmpty())
			{
				ShowBoundaryFeedback(FText::FromString(ConstructionPreview.FailureReason));
			}
			else
			{
				ShowBoundaryFeedback(NSLOCTEXT(
					"RokBuildCity",
					"LockedCardBoundary",
					"该建筑当前不可建造，未改变城市状态。"));
			}
			return;
		}
		if (bPlacementTarget)
		{
			if (!OnBuildCityPlacementRequested.IsBound())
			{
				ShowBoundaryFeedback(NSLOCTEXT(
					"RokBuildCity",
					"PlacementListenerBoundary",
					"该建筑暂时无法放置。"));
				return;
			}
			// Source closes BuildCity before asking the city scene to create a
			// temporary building. No construction command or resource mutation
			// occurs in this UI bridge.
			CloseScreen();
			OnBuildCityPlacementRequested.Broadcast(TypeId);
			return;
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok build city route: placement boundary type=%d."),
			TypeId);
		ShowBoundaryFeedback(FText::Format(
			NSLOCTEXT(
				"RokBuildCity",
				"PlacementBoundary",
				"已选择 {0}（类型 {1}）。网格占用、临时建筑与建造命令尚未接入；未生成建筑、未扣资源、未创建队列。"),
			ResolveBuildingTypeName(TypeId),
			FText::AsNumber(TypeId)));
		return;
	}
	if (ActionId == 1399)
	{
		return;
	}
	if (ActionId == 1400)
	{
		URokBuildingUpgradeSubsystem* UpgradeSubsystem = nullptr;
		if (UWorld* World = GetWorld())
		{
			UpgradeSubsystem =
				World->GetSubsystem<URokBuildingUpgradeSubsystem>();
		}
		if (!UpgradeSubsystem)
		{
			ShowBoundaryFeedback(
				NSLOCTEXT(
					"RokWorkerQueue",
					"QueueRuntimeBoundary",
					"建造运行态未加载，无法打开实际队列。"));
		}
		else if (AActor* ActiveUpgradeActor =
				UpgradeSubsystem->GetFirstActiveUpgradeActor())
		{
			const FRokBuildingState State = UpgradeSubsystem
				->GetBuildingStateForActor(ActiveUpgradeActor);
			if (State.bUpgradeInProgress)
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("Rok worker queue route: speed_up actor=%s."),
					*GetNameSafe(ActiveUpgradeActor));
				OpenSpeedUp(ActiveUpgradeActor, 20102);
			}
			else
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("Rok worker queue route: build_city after completed queue."));
				OpenBuildCity();
			}
		}
		else
		{
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok worker queue route: build_city from idle queue."));
			OpenBuildCity();
		}
		return;
	}
	if (ActionId == 1420)
	{
		ShowBoundaryFeedback(NSLOCTEXT(
			"RokWorkerQueue",
			"SecondQueueUnlockOrExtendBoundary",
			"第二队列需由服务器判断当前应解锁还是续期，并确认支付与有效期；当前未创建订单、延长有效期或伪造队列状态。"));
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
		return;
	}
	if (ActionId >= 1920 && ActionId <= 1922)
	{
		if (ActiveMode != ERokUnityFeatureScreenMode::TrainArmy || bTrainArmyDetailVisible) return;
		const auto* SelectionCatalog = GetGameplayCatalogSubsystem();
		if (!SelectionCatalog) return;
		const auto SelectionRows = SelectionCatalog->GetTroopsForArmyType(SelectedTrainArmyType);
		const auto* Selection = SelectionRows.FindByPredicate([this](const auto& Row) { return Row.Level == SelectedTrainTier; });
		if (!Selection || Selection->StudyId != 0) return;
		int32 BuildingLevel = 1;
		if (FeatureContextActor.IsValid())
		{
			if (UWorld* World = GetWorld())
			{
				if (URokBuildingUpgradeSubsystem* UpgradeSubsystem =
						World->GetSubsystem<URokBuildingUpgradeSubsystem>())
				{
					BuildingLevel = FMath::Max(
						1,
						UpgradeSubsystem->GetBuildingStateForActor(
							FeatureContextActor.Get()).Level);
				}
			}
		}
		int32 TrainingCapacity = 1;
		if (URokUnityGameplayCatalogSubsystem* Catalog =
				GetGameplayCatalogSubsystem())
		{
			if (!Catalog->IsCatalogLoaded())
			{
				Catalog->LoadCatalog();
			}
			FRokUnityCatalogBuildingType BuildingType;
			if (Catalog->GetBuildingType(
				SelectedTrainArmyType + 8,
				BuildingType))
			{
				TrainingCapacity = FMath::Max(
					1,
					BuildingType.GetTrainingCapacity(BuildingLevel));
			}
		}
		if (ActionId == 1920)
		{
			SelectedTrainCount = FMath::Max(
				1,
				SelectedTrainCount - 1);
		}
		else if (ActionId == 1921)
		{
			SelectedTrainCount = FMath::Min(
				TrainingCapacity,
				SelectedTrainCount + 1);
		}
		else
		{
			SelectedTrainCount = TrainingCapacity;
		}
		RebuildModeContent();
		return;
	}
	if (ActionId == 1940)
	{
		if (ActiveMode != ERokUnityFeatureScreenMode::TrainArmy || bTrainArmyDetailVisible) return;
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
			[this](const FRokUnityCatalogTroop& Candidate)
			{
				return Candidate.Level == SelectedTrainTier;
			});
		if (!Troop)
		{
			ShowBoundaryFeedback(NSLOCTEXT(
				"RokTrainArmy",
				"NoTrainingSelection",
				"没有可提交的兵种配置。"));
			return;
		}
		if (Troop->StudyId != 0)
		{
			ShowBoundaryFeedback(FText::Format(
				NSLOCTEXT(
					"RokTrainArmy",
					"SubmitTechnologyLocked",
					"兵种 {0} 仍依赖科技 {1}，不能提交训练。"),
				FText::AsNumber(Troop->Id),
				FText::AsNumber(Troop->StudyId)));
			return;
		}
		ShowBoundaryFeedback(FText::Format(
			NSLOCTEXT(
				"RokTrainArmy",
				"TrainingBoundary",
				"已选择兵种 {0} × {1}；Role_TrainArmy、资源扣除与服务器训练队列尚未接入，未伪造训练成功。"),
			FText::AsNumber(Troop->Id),
			FText::AsNumber(SelectedTrainCount)));
		return;
	}
	if (ActiveMode == ERokUnityFeatureScreenMode::Hospital && ActionId == 2004) { SetHospitalExplanationVisible(false); return; }
	if (ActiveMode == ERokUnityFeatureScreenMode::Hospital && bHospitalExplanationVisible && ActionId >= 2000 && ActionId <= 2003) return;
	if (ActionId >= 2000 && ActionId <= 2003)
	{
		if (ActionId == 2000)
		{
			SetHospitalExplanationVisible(true);
		}
		else if (ActionId == 2003)
		{
			ShowBoundaryFeedback(NSLOCTEXT(
				"RokHospital",
				"MaximumBoundary",
				"最大治疗数量依赖服务器伤兵列表、医院总容量与治疗倍率，当前没有修改选择。"));
		}
		else
		{
			ShowBoundaryFeedback(ActionId == 2001
				? NSLOCTEXT(
					"RokHospital",
					"TreatmentBoundary",
					"Role_Treatment、资源扣除、服务器治疗队列与完成时间尚未接入，未伪造治疗成功。")
				: NSLOCTEXT(
					"RokHospital",
					"ImmediateTreatmentBoundary",
					"立即治疗所需宝石、服务器校验和 Role_Treatment 回包尚未接入，未伪造立即完成。"));
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
		ResourceSearchSliderFill->SetPercent(
			MaxLevel > 1
				? static_cast<float>(RoundedLevel - 1) / static_cast<float>(MaxLevel - 1)
				: 0.0f);
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
