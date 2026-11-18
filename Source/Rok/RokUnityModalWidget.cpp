#include "RokUnityModalWidget.h"
#include "RokDataPaths.h"
#include "RokCsv.h"
#include "RokGalleryButton.h"
#include "RokUiTextTable.h"
#include "RokModalNineSlice.h"
#include "RokUiWidgets.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBoxSlot.h"
#include "RokSpriteAtlas.h"
#include "Components/Image.h"
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
#include "RokBuildingPreviewGlowWidget.h"
#include "Materials/MaterialInterface.h"

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
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
// Definition-list row icons: `atlas[sprite]` ids resolved through the `.tpsheet`
// manifests, decoded once. Unity tpsheet rects are measured from the bottom; UE V=0 is the top.
bool ResolveDefinitionIcon(const FString& IconId, UTexture2D*& OutSheet, FBox2D& OutUv)
{
	OutSheet = nullptr;
	if (IconId.IsEmpty())
	{
		return false;
	}
	static TSharedPtr<FRokSpriteAtlas> IconAtlas;
	if (!IconAtlas.IsValid())
	{
		IconAtlas = MakeShared<FRokSpriteAtlas>();
		IconAtlas->LoadAll();
	}
	FRokSpriteRect SpriteRect;
	if (!IconAtlas->Resolve(IconId, SpriteRect))
	{
		return false;
	}
	OutSheet = IconAtlas->GetSheetTexture(SpriteRect.SheetPng);
	if (!OutSheet)
	{
		return false;
	}
	OutUv = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
	if (SpriteRect.SheetWidth > 0 && SpriteRect.SheetHeight > 0)
	{
		const float U0 = static_cast<float>(SpriteRect.X) / SpriteRect.SheetWidth;
		const float V0 = static_cast<float>(SpriteRect.SheetHeight - SpriteRect.Y - SpriteRect.Height) / SpriteRect.SheetHeight;
		const float V1 = static_cast<float>(SpriteRect.SheetHeight - SpriteRect.Y) / SpriteRect.SheetHeight;
		const float U1 = static_cast<float>(SpriteRect.X + SpriteRect.Width) / SpriteRect.SheetWidth;
		OutUv = FBox2D(FVector2D(U0, V0), FVector2D(U1, V1));
	}
	return true;
}

void SetOptionalRowText(UTextBlock* Text, const FString& Value)
{
	if (!Text)
	{
		return;
	}
	if (Value.IsEmpty())
	{
		// The C++ build omitted empty detail/tag text; a collapsed block draws nothing either.
		Text->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Text->SetText(FText::FromString(Value));
}

const TCHAR* MainMenuAtlasPath =TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_mainmenu.rok_hud_atlas_mainmenu");
const TCHAR* CommonAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common");
const TCHAR* PlayerHeadAtlasPath = TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_player_head.rok_hud_atlas_player_head");
const TCHAR* PlayerUiAtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_player.rok_fullscreen_player");
const TCHAR* FrameAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame");
const TCHAR* SettingAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_setting.rok_modal_atlas_setting");
const TCHAR* CivilizationAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_civilization.rok_modal_atlas_civilization");
const TCHAR* CivilizationImageAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_civilization_img.rok_modal_atlas_civilization_img");
const TCHAR* CivilizationBackgroundAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_hero_background.rok_modal_atlas_hero_background");
const TCHAR* PageAtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_page.rok_modal_atlas_page");
const TCHAR* Item1AtlasPath = TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_item1.rok_modal_atlas_item1");
const TCHAR* Item5AtlasPath = TEXT("/Game/RokPrototype/UI/FullScreenAtlases/rok_fullscreen_item5.rok_fullscreen_item5");

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
const FIntPoint Item1AtlasSize(256, 1024);
const FIntPoint Item5AtlasSize(1024, 2048);

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

struct FModalShellWidgets
{
	UButton* DismissMask = nullptr;
	UButton* AnimationBlocker = nullptr;
	USizeBox* DesignSize = nullptr;
	UCanvasPanel* DesignCanvas = nullptr;
	UCanvasPanel* Content = nullptr;
};

bool FindModalShellWidgets(const UWidgetTree* Tree, FModalShellWidgets& Shell)
{
	const auto* Root = Tree ? Cast<UCanvasPanel>(Tree->RootWidget) : nullptr;
	if (!Root || Root->GetFName() != TEXT("RokUnityModalRoot"))
	{
		return false;
	}
	Shell.DismissMask = Tree->FindWidget<UButton>(TEXT("ModalDismissMask"));
	Shell.AnimationBlocker = Tree->FindWidget<UButton>(TEXT("ModalAnimationInputBlocker"));
	Shell.DesignSize = Tree->FindWidget<USizeBox>(TEXT("ModalExpandedDesignSize"));
	Shell.DesignCanvas = Tree->FindWidget<UCanvasPanel>(TEXT("ModalExpandedDesignCanvas"));
	Shell.Content = Tree->FindWidget<UCanvasPanel>(TEXT("ModalModeCanvas"));
	const auto* Dimmer = Tree->FindWidget<UBorder>(TEXT("ModalDimmer"));
	const auto* ScaleBox = Tree->FindWidget<UScaleBox>(TEXT("ModalUnityExpandScaleBox"));
	return Shell.DismissMask && Shell.DismissMask->GetParent() == Root
		&& Dimmer && Shell.DismissMask->GetContent() == Dimmer
		&& Shell.AnimationBlocker && Shell.AnimationBlocker->GetParent() == Root
		&& ScaleBox && ScaleBox->GetParent() == Root
		&& Shell.DesignSize && ScaleBox->GetContent() == Shell.DesignSize
		&& Shell.DesignCanvas && Shell.DesignSize->GetContent() == Shell.DesignCanvas
		&& Shell.Content && Shell.Content->GetParent() == Shell.DesignCanvas;
}

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

// "Base_Index": the names the page WBPs use for repeated widgets.
FName IndexedName(const TCHAR* Base, int32 Index)
{
	return FName(*FString::Printf(TEXT("%s_%d"), Base, Index));
}

void SetPageText(const URokUiPageWidget* Page, FName Name, const FText& Text)
{
	if (UTextBlock* TextBlock = Page->Get<UTextBlock>(Name))
	{
		TextBlock->SetText(Text);
	}
}

// How many fixed list slots (Base_0, Base_1, ...) a page was designed with. Logs
// when the data has more rows than the page can show.
int32 CountPageSlots(const URokUiPageWidget* Page, const TCHAR* Base, int32 DataRows)
{
	int32 Slots = 0;
	while (RokUiFind::Find(Page, IndexedName(Base, Slots)))
	{
		++Slots;
	}
	if (DataRows > Slots)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok UI: page %s has %d %s slots for %d rows; the rest are not shown"),
			*Page->GetClass()->GetName(), Slots, Base, DataRows);
	}
	return Slots;
}

// One city_buff_reference.json definition as the reference previews show it.
struct FCityBuffPreviewRow
{
	FString Title;
	int32 ConfigDurationSeconds = 0;
	FIntRect IconRect;
};

// The first MaxRows definitions with a valid main-menu icon; empty when the
// published configuration is missing or has another schema.
TArray<FCityBuffPreviewRow> LoadCityBuffPreviewRows(int32 MaxRows)
{
	TArray<FCityBuffPreviewRow> Rows;
	FString DefinitionJson;
	TSharedPtr<FJsonObject> DefinitionRoot;
	const TArray<TSharedPtr<FJsonValue>>* Definitions = nullptr;
	FString DefinitionSchema;
	if (!FFileHelper::LoadFileToString(DefinitionJson, *RokDataPaths::Resolve(TEXT("city_buff_reference.json")))
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(DefinitionJson), DefinitionRoot)
		|| !DefinitionRoot.IsValid()
		|| !DefinitionRoot->TryGetStringField(TEXT("schema"), DefinitionSchema)
		|| DefinitionSchema != TEXT("rok.city_buff_reference.v2")
		|| !DefinitionRoot->TryGetArrayField(TEXT("definitions"), Definitions)
		|| !Definitions)
	{
		return Rows;
	}
	for (const TSharedPtr<FJsonValue>& DefinitionValue : *Definitions)
	{
		if (Rows.Num() >= MaxRows)
		{
			break;
		}
		const TSharedPtr<FJsonObject> Definition = DefinitionValue.IsValid() ? DefinitionValue->AsObject() : nullptr;
		double ConfigDuration = 0.0;
		FString TitleCn;
		FString IconAtlas;
		const TArray<TSharedPtr<FJsonValue>>* IconRectValues = nullptr;
		if (!Definition.IsValid()
			|| !Definition->TryGetNumberField(TEXT("config_duration_seconds"), ConfigDuration)
			|| !Definition->TryGetStringField(TEXT("title_cn"), TitleCn)
			|| !Definition->TryGetStringField(TEXT("icon_atlas"), IconAtlas)
			|| !Definition->TryGetArrayField(TEXT("icon_rect"), IconRectValues)
			|| ConfigDuration < 0 || IconAtlas != MainMenuAtlasPath
			|| !IconRectValues || IconRectValues->Num() != 4)
		{
			continue;
		}
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
			|| IconNumbers[1] + IconNumbers[3] > MainMenuAtlasSize.Y)
		{
			continue;
		}
		FCityBuffPreviewRow& Row = Rows.AddDefaulted_GetRef();
		Row.Title = TitleCn.IsEmpty() ? FString(TEXT("名称本地化不可用")) : TitleCn;
		Row.ConfigDurationSeconds = static_cast<int32>(ConfigDuration);
		Row.IconRect = SpriteRect(static_cast<int32>(IconNumbers[0]), static_cast<int32>(IconNumbers[1]),
			static_cast<int32>(IconNumbers[2]), static_cast<int32>(IconNumbers[3]));
	}
	return Rows;
}

// The texts of one preview row in a page: <Prefix>Title_<i> and <Prefix>Fields_<i>.
void SetCityBuffPreviewRowText(const URokUiPageWidget* Page, const TCHAR* Prefix, int32 RowIndex, const FCityBuffPreviewRow& Row)
{
	SetPageText(Page, IndexedName(*FString::Printf(TEXT("%sTitle"), Prefix), RowIndex), FText::FromString(Row.Title));
	SetPageText(Page, IndexedName(*FString::Printf(TEXT("%sFields"), Prefix), RowIndex),
		FText::FromString(FString::Printf(TEXT("配置时长 %d 秒（非剩余时间）"), Row.ConfigDurationSeconds)));
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
	// The generic building layout (not the resource-building pages) is drawn.
	return ActivePage && ActivePageName == TEXT("BuildingInfo")
		&& RokUiFind::Find(ActivePage, TEXT("BuildingInfoPreviewFrame"))
		&& RokUiFind::Find(ActivePage, TEXT("BuildingInfoAttributeFrame"));
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

void URokUnityModalWidget::OpenGainEffect(int32 TabIndex)
{
	// UI_Win_GainEffect is the town hall's third menu action
	// (BuildingMenuData[129] -> openUI 1001 -> UI.s_GainEffect).
	ActiveMode = ERokUnityModalMode::GainEffect;
	GainEffectTabIndex = TabIndex == 1 ? 1 : 0;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::SampleWindowAnimationForCapture(float Seconds)
{
	if (!IsModalOpen())
	{
		return;
	}
	// Restart the source open curve, then advance to the requested time so the
	// screenshot sees 1.05/1.1/1.0 rather than the already-settled window.
	WindowAnimation.Open(WindowAnimation.GetStyle());
	AdvanceWindowAnimation(FMath::Max(0.0f, Seconds));
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok modal animation sample: %.4fs scale=%.4f opacity=%.4f."),
		Seconds,
		WindowAnimation.GetSample().Scale,
		WindowAnimation.GetSample().Opacity);
}


bool URokUnityModalWidget::FreezePreviewAnimationForCapture(float Seconds)
{
	if (!IsModalOpen() || !FMath::IsFinite(Seconds) || Seconds < 0.0f)
	{
		return false;
	}
	if (ActiveMode == ERokUnityModalMode::CivilizationSelection)
	{
		if (!CivilizationHero || !CivilizationListRoot || !CivilizationDetailRoot || !CivilizationSelectionPulse
			|| !CivilizationHero->FreezeAnimationForCapture(Seconds))
		{
			return false;
		}
		CivilizationListAnimationTime = FMath::Min(Seconds, 0.5f);
		CivilizationHeroAnimationTime = FMath::Min(Seconds, RokCivilizationAnimation::ShowDuration + RokCivilizationAnimation::OutDuration);
		CivilizationSelectionAnimationTime = FMath::Fmod(Seconds, RokCivilizationAnimation::SelectionDuration);
		ApplyWindowAnimation();
		bPreviewAnimationFrozenForCapture = true;
		UE_LOG(LogTemp, Display, TEXT("Rok modal preview capture: civilization sampled at %.6fs, flag pulse phase %.6fs."),
			Seconds, CivilizationSelectionAnimationTime);
		return true;
	}
	if (auto* Glow = Cast<URokBuildingPreviewGlowWidget>(RokUiFind::Find(this, TEXT("BuildingPreviewGlowWidget"))))
	{
		if (Glow->IsPlaybackActive() && Glow->SetPlaybackTime(Seconds))
		{
			Glow->SetAutoAdvance(false);
			bPreviewAnimationFrozenForCapture = true;
			UE_LOG(LogTemp, Display, TEXT("Rok modal preview capture: building glow sampled at %.6fs, source phase %.6fs."),
				Glow->GetPlaybackTime(), Glow->GetSourcePhase());
			return true;
		}
	}
	return false;
}

void URokUnityModalWidget::OpenBattlePassRewards()
{
	// UI_Win_BattlePassRewards: the source lists one row per battle-pass level
	// with a free and a paid track reward. Local definition preview only.
	ActiveMode = ERokUnityModalMode::BattlePassRewards;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenEventDate()
{
	// UI_Win_EventDate: activity schedule window. Local definition preview.
	ActiveMode = ERokUnityModalMode::EventDate;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}
void URokUnityModalWidget::OpenGuildPurview()
{
	// UI_Win_GuildPurview: alliance permission matrix. Local definition preview.
	ActiveMode = ERokUnityModalMode::GuildPurview;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}
void URokUnityModalWidget::OpenChapterDialog(int32 GroupId)
{
	// UI_Win_ChapterDialog: GroupId == 0 lists chapters, otherwise that TaskDialog group's lines.
	ChapterDialogGroup = FMath::Max(0, GroupId);
	ActiveMode = ERokUnityModalMode::ChapterDialog;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}
void URokUnityModalWidget::OpenGuildFortress()
{
	// UI_Win_GuildFortress: alliance structure costs. Local definition preview.
	ActiveMode = ERokUnityModalMode::GuildFortress;
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
	++UpgradeViewSerial;
	bServerUpgradePending = false;
	ConfirmedInstantQuote.Reset();
	ClosePlayerKillTip();
	ClosePlayerPowerActionTip();
	bPreviewAnimationFrozenForCapture = false;
	WindowAnimation.Reset();
	BuffListAnimationRoot = nullptr;
	GainEffectListRoot = nullptr;
	GainEffectTabIndex = 0;
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
	ActivePage = nullptr;
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

TSubclassOf<URokUnityModalWidget> URokUnityModalWidget::ResolveShellClass()
{
	static const TSoftClassPtr<URokUnityModalWidget> LayoutClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Modal/WBP_RokModalShell.WBP_RokModalShell_C")));
	auto* GeneratedClass = Cast<UWidgetBlueprintGeneratedClass>(URokUiSettings::Resolve(LayoutClass));
	FModalShellWidgets Shell;
	return GeneratedClass && FindModalShellWidgets(GeneratedClass->GetWidgetTreeArchetype(), Shell)
		? GeneratedClass : StaticClass();
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
	if (!bPreviewAnimationFrozenForCapture)
	{
		AdvanceWindowAnimation(InDeltaTime);
	}
	UpgradeButtonRefreshRemaining -= InDeltaTime;
	if (ActiveMode == ERokUnityModalMode::BuildingUpgrade && SelectedBuildingActor.IsValid()
		&& UpgradeButtonRefreshRemaining <= 0.0f)
	{
		RefreshUpgradeButtons();
	}
}

void URokUnityModalWidget::BuildWidgetTree()
{
	if (!WidgetTree || ModeCanvas)
	{
		return;
	}
	if (WidgetTree->RootWidget)
	{
		FModalShellWidgets Shell;
		if (FindModalShellWidgets(WidgetTree, Shell))
		{
			ExpandedDesignSizeBox = Shell.DesignSize;
			ExpandedDesignCanvas = Shell.DesignCanvas;
			ModeCanvas = Shell.Content;
			AnimationInputBlocker = Shell.AnimationBlocker;
			ExpandedDesignSizeBox->SetWidthOverride(CurrentExpandedDesignSize.X);
			ExpandedDesignSizeBox->SetHeightOverride(CurrentExpandedDesignSize.Y);
			Shell.DismissMask->OnClicked.AddUniqueDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
		}
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
	// Center the scaled design like Unity's CanvasScaler; in UE the alignment
	// lives on the ScaleBox slot rather than on the box itself.
	if (UScaleBoxSlot* DesignSlot = Cast<UScaleBoxSlot>(ExpandedDesignSizeBox->Slot))
	{
		DesignSlot->SetHorizontalAlignment(HAlign_Center);
		DesignSlot->SetVerticalAlignment(VAlign_Center);
	}

	ExpandedDesignCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ModalExpandedDesignCanvas"));
	ExpandedDesignSizeBox->SetContent(ExpandedDesignCanvas);

	ModeCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ModalModeCanvas"));
	ModeCanvas->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	UCanvasPanelSlot* ModeSlot = ExpandedDesignCanvas->AddChildToCanvas(ModeCanvas);
	// Unity CanvasScaler "expand" keeps the reference height and widens the canvas,
	// so UI anchored to the centre stays centred. Our builders place absolute
	// coordinates in the 1440x810 reference area, hence a centred mode canvas.
	ModeSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	ModeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ModeSlot->SetPosition(FVector2D::ZeroVector);
	ModeSlot->SetSize(FVector2D(UnityReferenceWidth, UnityReferenceHeight));

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
	ActivePage = nullptr;
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

	if (TryMountPageOverride())
	{
		UpdateWindowAnimationForMode(bOpening);
		return;
	}

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
	case ERokUnityModalMode::GainEffect:
		BuildType2Window(NSLOCTEXT("RokModal", "GainEffectTitle", "增益效果"));
		BuildGainEffect();
		break;
	case ERokUnityModalMode::EventDate:
		BuildType2Window(NSLOCTEXT("RokModal", "EventDateTitle", "活动日程"));
		BuildEventDate();
		break;
	case ERokUnityModalMode::GuildPurview:
		BuildType2Window(NSLOCTEXT("RokModal", "GuildPurviewTitle", "联盟权限"));
		BuildGuildPurview();
		break;
	case ERokUnityModalMode::VipPrivilege:
		BuildType2Window(NSLOCTEXT("RokModal", "VipPrivilegeTitle", "贵宾特权"));
		BuildVipPrivilege();
		break;
		case ERokUnityModalMode::UiGallery:
		BuildType2Window(NSLOCTEXT("RokModal", "UiGalleryTitle", "界面一览"));
		BuildUiGallery();
		break;
		case ERokUnityModalMode::ItemExchange:
		BuildType2Window(NSLOCTEXT("RokModal", "ItemExchangeTitle", "自选宝箱"));
		BuildItemExchange();
		break;
		case ERokUnityModalMode::GuildGift:
		BuildType2Window(NSLOCTEXT("RokModal", "GuildGiftTitle", "联盟礼物"));
		BuildGuildGift();
		break;
		case ERokUnityModalMode::FuncGuide:
		BuildType2Window(NSLOCTEXT("RokModal", "FuncGuideTitle", "功能引导"));
		BuildFuncGuide();
		break;
		case ERokUnityModalMode::TavernReward:
		BuildType2Window(NSLOCTEXT("RokModal", "TavernRewardTitle", "酒馆奖励"));
		BuildTavernReward();
		break;
		case ERokUnityModalMode::ExpeditionStore:
		BuildType2Window(NSLOCTEXT("RokModal", "ExpeditionStoreTitle", "远征商店"));
		BuildExpeditionStore();
		break;
		case ERokUnityModalMode::ChargePop:
		BuildType2Window(NSLOCTEXT("RokModal", "ChargePopTitle", "充值礼包"));
		BuildChargePop();
		break;
		case ERokUnityModalMode::GiftLimitedTime:
		BuildType2Window(NSLOCTEXT("RokModal", "GiftLimitedTimeTitle", "限时礼包"));
		BuildGiftLimitedTime();
		break;
		case ERokUnityModalMode::GuildStore:
		BuildType2Window(NSLOCTEXT("RokModal", "GuildStoreTitle", "联盟商店"));
		BuildGuildStore();
		break;
		case ERokUnityModalMode::TheTower:
		BuildType2Window(NSLOCTEXT("RokModal", "TheTowerTitle", "警戒塔"));
		BuildTheTower();
		break;
		case ERokUnityModalMode::TheWall:
		BuildType2Window(NSLOCTEXT("RokModal", "TheWallTitle", "城墙"));
		BuildTheWall();
		break;
		case ERokUnityModalMode::TavernBoxDesc:
		BuildType2Window(NSLOCTEXT("RokModal", "TavernBoxDescTitle", "酒馆概率"));
		BuildTavernBoxDesc();
		break;
		case ERokUnityModalMode::VipStore:
		BuildType2Window(NSLOCTEXT("RokModal", "VipStoreTitle", "贵宾商店"));
		BuildVipStore();
		break;
		case ERokUnityModalMode::ItemCollection:
		BuildType2Window(NSLOCTEXT("RokModal", "ItemCollectionTitle", "道具图鉴"));
		BuildItemCollection();
		break;
		case ERokUnityModalMode::GuildFortress:
		BuildType2Window(NSLOCTEXT("RokModal", "GuildFortressTitle", "联盟堡垒"));
		BuildGuildFortress();
		break;
		case ERokUnityModalMode::ChapterDialog:
		BuildType2Window(ChapterDialogGroup > 0
			? NSLOCTEXT("RokModal", "ChapterDialogScreenTitle", "剧情对话")
			: NSLOCTEXT("RokModal", "ChapterDialogTitle", "剧情章节"));
		BuildChapterDialog();
		break;
	case ERokUnityModalMode::BattlePassRewards:
		BuildType2Window(NSLOCTEXT("RokModal", "BattlePassRewardsTitle", "战令奖励"));
		BuildBattlePassRewards();
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

void URokUnityModalWidget::MountFullPiece(UUserWidget* Piece, int32 ZOrder)
{
	UCanvasPanelSlot* PieceSlot = ModeCanvas->AddChildToCanvas(Piece);
	PieceSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	PieceSlot->SetOffsets(FMargin(0.0f));
	PieceSlot->SetZOrder(ZOrder);
}

URokModalWindowFrame* URokUnityModalWidget::MountWindowFrame(
	const TSoftClassPtr<URokModalWindowFrame>& FrameSetting,
	const FText& Title)
{
	UClass* FrameClass = URokUiSettings::Resolve(FrameSetting);
	URokModalWindowFrame* Frame = FrameClass ? CreateWidget<URokModalWindowFrame>(this, FrameClass) : nullptr;
	if (Frame)
	{
		Frame->Title->SetText(Title);
		MountFullPiece(Frame, 0);
	}
	return Frame;
}

URokUiPageWidget* URokUnityModalWidget::CreateStatusMessage(FName MessageName,
	FName LayoutName, const FText& Message)
{
	static const TSoftClassPtr<URokUiPageWidget> StatusClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Modal/Items/WBP_RokModal_StatusMessages.WBP_RokModal_StatusMessages_C")));
	static const TSoftClassPtr<URokUiPageWidget> UnavailableClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Modal/Items/WBP_RokModal_UnavailableStatusMessages.WBP_RokModal_UnavailableStatusMessages_C")));
	const bool bUnavailableLayout = LayoutName == TEXT("PlayerHeadCatalogError")
		|| LayoutName == TEXT("PlayerRankingCatalogError") || LayoutName == TEXT("BarracksAttributeError")
		|| LayoutName == TEXT("BuffDefinitionError");
	UClass* Class = URokUiSettings::Resolve(bUnavailableLayout ? UnavailableClass : StatusClass);
	auto* Page = Class ? CreateWidget<URokUiPageWidget>(this, Class) : nullptr;
	auto* Root = Page ? Cast<UCanvasPanel>(Page->GetRootWidget()) : nullptr;
	auto* Label = Page ? Page->Get<UTextBlock>(LayoutName) : nullptr;
	if (!Root || !Label || Label->GetParent() != Root)
	{
		return nullptr;
	}
	// The template owns geometry and base style; keep legacy lookup identity
	// without changing its designer-authored widget names or the active mode page.
	Page->LegacyNames.Add(LayoutName, MessageName.ToString());
	for (UWidget* Other : Root->GetAllChildren())
	{
		if (Cast<UTextBlock>(Other) && Other != Label)
		{
			Other->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	Label->SetText(Message);
	Label->SetVisibility(ESlateVisibility::HitTestInvisible);
	Page->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Page;
}

bool URokUnityModalWidget::TryMountStatusMessage(UCanvasPanel* Parent, FName MessageName,
	FName LayoutName, const FText& Message)
{
	auto* Page = Parent ? CreateStatusMessage(MessageName, LayoutName, Message) : nullptr;
	if (!Page)
	{
		return false;
	}
	auto* Slot = Parent->AddChildToCanvas(Page);
	Slot->SetAnchors(FAnchors(0, 0, 1, 1));
	Slot->SetOffsets(FMargin(0));
	Slot->SetZOrder(LayoutName == TEXT("BarracksHeaderError") ? 5
		: (LayoutName == TEXT("BuildingLevelError") || LayoutName == TEXT("PlayerRankingCatalogError")) ? 0 : 3);
	return true;
}

void URokUnityModalWidget::AddBarracksAttributeUnavailable(UCanvasPanel* Parent, const FVector2D& Position,
	const FVector2D& Size, int32 ZOrder)
{
	const FName Name(TEXT("BarracksUpgradeAttributeUnavailable"));
	const FText Message = NSLOCTEXT("RokModal", "BarracksAttributeUnavailable", "兵营升级属性源数据不可用");
	if (auto* Page = CreateStatusMessage(Name, TEXT("BarracksAttributeError"), Message))
	{
		PlaceOnCanvas(Parent, Page, Position, Size, ZOrder);
	}
	else
	{
		auto* Error = MakeModalText(Name, 15, FLinearColor::White);
		Error->SetText(Message);
		PlaceOnCanvas(Parent, Error, Position, Size, ZOrder);
	}
}

URokUiPageWidget* URokUnityModalWidget::MountModePage(FName Page)
{
	UClass* PageClass = URokUiSettings::ResolvePage(GetDefault<URokUiSettings>()->ModalPages, Page);
	URokUiPageWidget* Widget = PageClass ? CreateWidget<URokUiPageWidget>(this, PageClass) : nullptr;
	if (Widget)
	{
		// Above the window frame (z 0), where the C++ mode content used to sit.
		MountFullPiece(Widget, 1);
	}
	ActivePage = Widget;
	ActivePageName = Widget ? Page : NAME_None;
	return Widget;
}

URokUiPageWidget* URokUnityModalWidget::MountItem(
	FName Item,
	UCanvasPanel* Parent,
	FName InstanceName,
	const FVector2D& Position,
	const FVector2D& Size,
	int32 ZOrder,
	const FString& Prefix,
	const FString& Suffix,
	const FString& Index)
{
	UClass* ItemClass = Parent
		? URokUiSettings::ResolvePage(GetDefault<URokUiSettings>()->ModalPages, Item) : nullptr;
	if (!ItemClass)
	{
		return nullptr;
	}
	// A previous open's instance of the same name may still be outered here.
	if (UObject* Existing = StaticFindObjectFast(nullptr, Parent->GetOuter(), InstanceName))
	{
		RokUiFind::Retire(Existing);
	}
	URokUiPageWidget* Widget = CreateWidget<URokUiPageWidget>(Parent, ItemClass, InstanceName);
	Widget->LegacyPrefix = Prefix;
	Widget->LegacySuffix = Suffix;
	Widget->LegacyIndex = Index;
	PlaceOnCanvas(Parent, Widget, Position, Size, ZOrder);
	return Widget;
}

void URokUnityModalWidget::BindIndexedButton(UWidget* Button, int32 ActionId)
{
	if (URokUnityIndexedButton* Indexed = Cast<URokUnityIndexedButton>(Button))
	{
		Indexed->ConfigureAction(ActionId);
		Indexed->OnIndexedClicked.AddUObject(this, &URokUnityModalWidget::HandleProfileSubscreenAction);
	}
	else if (Button)
	{
		UE_LOG(LogTemp, Error, TEXT("Rok UI: %s is a %s, not an indexed button"), *Button->GetName(), *Button->GetClass()->GetName());
	}
}

URokUiPageWidget* URokUnityModalWidget::MountTipLayer(FName Item, FName InstanceName)
{
	// Over the mode page, filling the expanded canvas so a click anywhere else closes it.
	URokUiPageWidget* Layer = MountItem(Item, ModeCanvas, InstanceName, FVector2D::ZeroVector,
		CurrentExpandedDesignSize, 100, FString(), FString());
	if (Layer)
	{
		auto* LayerSlot = CastChecked<UCanvasPanelSlot>(Layer->Slot);
		LayerSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		LayerSlot->SetOffsets(FMargin(0));
	}
	return Layer;
}

void URokUnityModalWidget::SetAtlasSprite(
	UImage* Image,
	const TCHAR* TexturePath,
	const FIntRect& UnityRect,
	const FIntPoint& AtlasSize,
	const FVector2D& DesiredSize) const
{
	if (!Image)
	{
		return;
	}
	if (UTexture2D* Texture = LoadUiTexture(TexturePath))
	{
		Image->SetBrush(MakeAtlasBrush(Texture, UnityRect, AtlasSize, DesiredSize));
	}
}

bool URokUnityModalWidget::TryMountPageOverride()
{
	FString PagePath;
	if (ActiveMode == ERokUnityModalMode::None
		|| !FParse::Value(FCommandLine::Get(), TEXT("RokUiModalPage="), PagePath)
		|| PagePath.IsEmpty())
	{
		return false;
	}
	UClass* PageClass = LoadClass<UUserWidget>(nullptr, *PagePath);
	if (!PageClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Rok UI page override: class %s not found, using the C++ build"), *PagePath);
		return false;
	}
	UUserWidget* Page = CreateWidget<UUserWidget>(this, PageClass);
	if (!Page)
	{
		return false;
	}
	MountFullPiece(Page, 0);
	UE_LOG(LogTemp, Display, TEXT("Rok UI page override: mounted %s for mode %d"), *PagePath, static_cast<int32>(ActiveMode));
	return true;
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
		case ERokUnityModalMode::GainEffect:
		case ERokUnityModalMode::EventDate:
		case ERokUnityModalMode::VipPrivilege:
		case ERokUnityModalMode::UiGallery:
		case ERokUnityModalMode::ItemExchange:
		case ERokUnityModalMode::GuildGift:
		case ERokUnityModalMode::FuncGuide:
		case ERokUnityModalMode::TavernReward:
		case ERokUnityModalMode::ExpeditionStore:
		case ERokUnityModalMode::ChargePop:
		case ERokUnityModalMode::GiftLimitedTime:
		case ERokUnityModalMode::GuildStore:
		case ERokUnityModalMode::TheTower:
		case ERokUnityModalMode::TheWall:
		case ERokUnityModalMode::TavernBoxDesc:
		case ERokUnityModalMode::VipStore:
		case ERokUnityModalMode::ItemCollection:
		case ERokUnityModalMode::GuildFortress:
		case ERokUnityModalMode::ChapterDialog:
		case ERokUnityModalMode::BattlePassRewards:
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
	if (URokModalWindowFrame* Frame = MountWindowFrame(GetDefault<URokUiSettings>()->ModalWindowType1, Title))
	{
		Frame->CloseButton->OnClicked.AddUniqueDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
		return;
	}
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
	const URokUiSettings* Settings = GetDefault<URokUiSettings>();
	if (bSourceBarracksUpgradeParent)
	{
		// The barracks preview overlaps the header, so the frame straddles the content at Z=1.
		UClass* BodyClass = URokUiSettings::Resolve(Settings->ModalWindowType2BarracksBody);
		UClass* HeaderClass = URokUiSettings::Resolve(Settings->ModalWindowType2Barracks);
		if (BodyClass && HeaderClass)
		{
			URokUiPageWidget* Body = CreateWidget<URokUiPageWidget>(this, BodyClass);
			URokModalWindowFrame* Header = CreateWidget<URokModalWindowFrame>(this, HeaderClass);
			if (Body && Header)
			{
				Body->SetVisibility(ESlateVisibility::HitTestInvisible);
				Header->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				Header->Title->SetText(Title);
				Header->CloseButton->OnClicked.AddUniqueDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
				MountFullPiece(Body, 0);
				MountFullPiece(Header, 2);
				return;
			}
		}
	}
	else if (URokModalWindowFrame* Frame = MountWindowFrame(Settings->ModalWindowType2, Title))
	{
		Frame->CloseButton->OnClicked.AddUniqueDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
		return;
	}

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
	if (URokModalWindowFrame* Frame = MountWindowFrame(GetDefault<URokUiSettings>()->ModalWindowType3, Title))
	{
		Frame->CloseButton->OnClicked.AddUniqueDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
		return;
	}
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
	if (URokModalWindowFrame* Frame = MountWindowFrame(GetDefault<URokUiSettings>()->ModalWindowCivilizationChange, Title))
	{
		Frame->CloseButton->OnClicked.AddUniqueDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
		return;
	}
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
	if (URokModalWindowFrame* Frame = MountWindowFrame(GetDefault<URokUiSettings>()->ModalWindowTypeMid, Title))
	{
		Frame->CloseButton->OnClicked.AddUniqueDynamic(this, &URokUnityModalWidget::HandleChangeNameCancelClicked);
		return;
	}
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

	// Page and data-presentation WBPs own layout and style. Native code supplies
	// the current role values, visibility, progress ratio and button handlers.
	URokUiPageWidget* Page = MountModePage(TEXT("PlayerProfile"));
	if (!Page)
	{
		return;
	}
	UCanvasPanel* PageCanvas = Cast<UCanvasPanel>(Page->GetRootWidget());
	static const TSoftClassPtr<URokUiPageWidget> DataClass(FSoftObjectPath(
		TEXT("/Game/RokUI/Modal/Items/WBP_RokModal_ProfileDataPresentation.WBP_RokModal_ProfileDataPresentation_C")));
	UClass* DataLayoutClass = URokUiSettings::Resolve(DataClass);
	auto* DataPage = PageCanvas && DataLayoutClass ? CreateWidget<URokUiPageWidget>(this, DataLayoutClass) : nullptr;
	auto* CivilizationIcon = DataPage ? DataPage->Get<UImage>(TEXT("CivilizationIcon")) : nullptr;
	auto* ActionFill = DataPage ? DataPage->Get<UProgressBar>(TEXT("ActionFill")) : nullptr;
	const bool bDataLayout = CivilizationIcon && ActionFill;
	if (bDataLayout)
	{
		DataPage->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* DataSlot = PageCanvas->AddChildToCanvas(DataPage);
		DataSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		DataSlot->SetOffsets(FMargin(0));
		DataSlot->SetZOrder(2);
		UTexture2D* CivilizationTexture = ProfileState.bHasCivilization && ProfileState.CivilizationId == 101
			? LoadUiTexture(CivilizationAtlasPath) : nullptr;
		if (CivilizationTexture)
		{
			// Reuse the existing runtime icon binding; the new template contains no atlas art.
			CivilizationIcon->SetBrush(MakeAtlasBrush(CivilizationTexture, CivilizationArabiaRect,
				CivilizationAtlasSize, FVector2D(53, 51)));
		}
		DataPage->SetPresent(CivilizationIcon, CivilizationTexture != nullptr);
		const bool bHasActionPoints = ProfileState.bHasActionPoints && ProfileState.ActionPointsMax > 0;
		ActionFill->SetPercent(bHasActionPoints ? FMath::Clamp(
			static_cast<float>(ProfileState.ActionPointsCurrent) / static_cast<float>(ProfileState.ActionPointsMax), 0.0f, 1.0f) : 0.0f);
		DataPage->SetPresent(ActionFill, bHasActionPoints);
	}
	if (UButton* Button = Page->Get<UButton>(TEXT("ProfilePictureButton"))) Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileAvatarClicked);
	if (UButton* Button = Page->Get<UButton>(TEXT("ProfileModifyButton"))) Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileAvatarClicked);
	if (UButton* Button = Page->Get<UButton>(TEXT("ProfileRenameButton"))) Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileRenameClicked);
	if (UButton* Button = Page->Get<UButton>(TEXT("ProfileCivilizationButton"))) Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileCivilizationClicked);
	if (UButton* Button = Page->Get<UButton>(TEXT("ProfileKillsHelpButton"))) Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileKillsHelpClicked);
	if (UButton* Button = Page->Get<UButton>(TEXT("ProfileActionHelpButton"))) Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileActionHelpClicked);
	if (UButton* Button = Page->Get<UButton>(TEXT("ActionAddButton"))) Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileActionAddClicked);
	if (UButton* Button = Page->Get<UButton>(TEXT("ProfileArmyButton"))) Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileArmyClicked);
	if (UButton* Button = Page->Get<UButton>(TEXT("ProfileMoreButton"))) Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileMoreClicked);
	if (UButton* Button = Page->Get<UButton>(TEXT("ProfileRankingButton"))) Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileRankingClicked);
	if (UButton* Button = Page->Get<UButton>(TEXT("ProfileSettingsButton"))) Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileSettingsClicked);

	SetPageText(Page, TEXT("RoleId"), ProfileState.bHasRoleId
		? FText::Format(NSLOCTEXT("RokModal", "RoleIdValue", "ID: {0}"), FText::FromString(ProfileState.RoleId))
		: NSLOCTEXT("RokModal", "RoleIdUnavailable", "ID: --"));
	SetPageText(Page, TEXT("ProfilePlayerName"), ProfileState.bHasDisplayName
		? FText::FromString(ProfileState.DisplayName)
		: NSLOCTEXT("RokModal", "PlayerNameUnavailable", "名称未接入"));
	// Only Arabia has a source icon in this atlas so far.
	if (!bDataLayout && PageCanvas && ProfileState.bHasCivilization && ProfileState.CivilizationId == 101)
	{
		PlaceOnCanvas(PageCanvas,
			MakeAtlasImage(TEXT("CivilizationIcon"), CivilizationAtlasPath, CivilizationArabiaRect, CivilizationAtlasSize, FVector2D(53.0f, 51.0f)),
			FVector2D(1053.0f, 243.0f), FVector2D(53.0f, 51.0f), 2);
	}
	SetPageText(Page, TEXT("CivilizationName"), ProfileState.bHasCivilization
		? FText::FromString(ProfileState.CivilizationName)
		: NSLOCTEXT("RokModal", "CivilizationUnavailable", "--"));
	SetPageText(Page, TEXT("AllianceValue"), ProfileState.bHasAlliance
		? FText::FromString(ProfileState.AllianceName)
		: NSLOCTEXT("RokModal", "AllianceUnavailable", "--"));
	SetPageText(Page, TEXT("PowerValue"), ProfileState.LocalPreviewScore > 0
		? FormatNumber(ProfileState.LocalPreviewScore)
		: NSLOCTEXT("RokModal", "LocalPreviewScoreUnavailable", "--"));
	SetPageText(Page, TEXT("KillsValue"), ProfileState.bHasKillCount
		? FText::AsNumber(ProfileState.KillCount)
		: NSLOCTEXT("RokModal", "KillsUnavailable", "--"));
	// Missing data-presentation assets retain the existing native fallback.
	if (!bDataLayout && PageCanvas && ProfileState.bHasActionPoints && ProfileState.ActionPointsMax > 0)
	{
		const float FillWidth = 528.15f * FMath::Clamp(
			static_cast<float>(ProfileState.ActionPointsCurrent) / static_cast<float>(ProfileState.ActionPointsMax), 0.0f, 1.0f);
		PlaceOnCanvas(PageCanvas,
			MakeAtlasBorder(TEXT("ActionFill"), CommonAtlasPath, ActionFillRect, CommonAtlasSize, FVector2D(FillWidth, 18.0f), ActionFillMargin),
			FVector2D(577.39f, 427.73f), FVector2D(FillWidth, 18.0f), 3);
	}
	SetPageText(Page, TEXT("ActionValue"), ProfileState.bHasActionPoints
		? FText::Format(NSLOCTEXT("RokModal", "ActionPointValue", "{0} / {1}"),
			FText::AsNumber(ProfileState.ActionPointsCurrent), FText::AsNumber(ProfileState.ActionPointsMax))
		: NSLOCTEXT("RokModal", "ActionPointUnavailable", "-- / --"));
	UE_LOG(LogTemp, Display, TEXT("Rok profile local score: total=%lld hero=%lld troop=%lld world_building_upgrade=%d authority=unsynchronized"),
		static_cast<long long>(ProfileState.LocalPreviewScore), static_cast<long long>(ProfileState.LocalHeroScore),
		static_cast<long long>(ProfileState.LocalTroopPower), ProfileState.LocalBuildingUpgradeScore);
	FeedbackText = Page->Get<UTextBlock>(TEXT("ProfileNavigationFeedback"));
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
		const FName Name(TEXT("PlayerHeadCatalogMissing"));
		const FText Message = NSLOCTEXT("RokModal", "PlayerHeadCatalogMissing", "头像目录不可用，请检查迁移数据。");
		if (!TryMountStatusMessage(ModeCanvas, Name, TEXT("PlayerHeadCatalogError"), Message))
		{
			auto* Missing = MakeModalText(Name, 24, FLinearColor::White);
			Missing->SetText(Message);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(300, 230), FVector2D(800, 60), 3);
		}
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

	// Grid backing, preview, both looks of each side tab and the confirm button
	// live in the page WBP; each catalog row is a cell WBP in the scrolling grid.
	URokUiPageWidget* Page = MountModePage(TEXT("PlayerHeadSelection"));
	if (!Page)
	{
		return;
	}
	int32 FreeCount = 0;
	for (const auto& Row : PlayerHeadPageRows) FreeCount += Row.bFree ? 1 : 0;
	SetPageText(Page, TEXT("PlayerHeadOwnedCount"), FText::Format(
		NSLOCTEXT("RokModal", "PlayerHeadCatalogCount", "默认可用：{0} / 目录：{1}"),
		FText::AsNumber(FreeCount), FText::AsNumber(PlayerHeadPageRows.Num())));
	PlayerHeadGridScroll = Page->Get<UScrollBox>(TEXT("PlayerHeadGridScroll"));
	if (PlayerHeadGridScroll)
	{
		PlayerHeadGridScroll->SetScrollOffset(PlayerHeadScrollOffset);
	}
	if (USizeBox* GridSize = Page->Get<USizeBox>(TEXT("PlayerHeadGridSize")))
	{
		GridSize->SetHeightOverride(ContentHeight(PlayerHeadPageRows.Num()));
	}
	// A row's art in an image, or no image when the row has none.
	const auto ShowArt = [this](const URokUiPageWidget* Owner, UImage* Image, const FRokPlayerHeadRow* Row, float Size)
	{
		const bool bArt = Row && Row->bArtAvailable;
		if (bArt)
		{
			SetAtlasSprite(Image, *Row->Texture, Row->Rect, Row->AtlasSize, FVector2D(Size));
		}
		Owner->SetPresent(Image, bArt);
	};
	UCanvasPanel* Grid = Page->Get<UCanvasPanel>(TEXT("PlayerHeadGrid"));
	for (int32 Index = 0; Grid && Index < PlayerHeadPageRows.Num(); ++Index)
	{
		const auto& Row = PlayerHeadPageRows[Index];
		URokUiPageWidget* Cell = MountItem(TEXT("PlayerHeadCell"), Grid, *FString::Printf(TEXT("PlayerHeadItem_%d"), Index),
			CellPosition(Index), FVector2D(135), 1, TEXT("PlayerHead"),
			FString::Printf(TEXT("_%d"), Row.Id), FString::Printf(TEXT("_%d"), Index));
		if (!Cell)
		{
			break;
		}
		// Unity img_selet is sibling 0, behind UI_Model_PlayerHead (sibling 1).
		// Its opaque center is covered by the portrait, leaving the glowing rim.
		Cell->SetShown(Cell->Get<UWidget>(TEXT("Cell")), Index == SelectedPlayerHeadIndex);
		ShowArt(Cell, Cell->Get<UImage>(TEXT("ChoiceImage")), SelectedPlayerHeadPage == 0 ? &Row : CurrentHead, 126.0f);
		ShowArt(Cell, Cell->Get<UImage>(TEXT("ChoiceFrame")), SelectedPlayerHeadPage == 1 ? &Row : nullptr, 126.0f);
		UTextBlock* Missing = Cell->Get<UTextBlock>(TEXT("MissingArt"));
		if (Missing && !Row.bArtAvailable)
		{
			Missing->SetText(FText::Format(NSLOCTEXT("RokModal", "PlayerHeadArtMissing", "{0}\n图像缺失"), FText::AsNumber(Row.Id)));
		}
		Cell->SetPresent(Missing, !Row.bArtAvailable);
		SetPageText(Cell, TEXT("Access"), Row.bFree
			? NSLOCTEXT("RokModal", "PlayerHeadFree", "默认可用")
			: NSLOCTEXT("RokModal", "PlayerHeadUnlockUnknown", "解锁状态待同步"));
		BindIndexedButton(Cell->Get<UWidget>(TEXT("ChoiceButton")), 3000 + Index);
	}
	ShowArt(Page, Page->Get<UImage>(TEXT("PlayerHeadPreviewImage")), SelectedPlayerHeadPage == 0 ? &Selected : CurrentHead, 195.3f);
	ShowArt(Page, Page->Get<UImage>(TEXT("PlayerHeadPreviewFrame")), SelectedPlayerHeadPage == 1 ? &Selected : CurrentFrame, 195.3f);
	Page->SetPresent(Page->Get<UWidget>(TEXT("PlayerHeadPreviewMissingArt")), !Selected.bArtAvailable);
	Page->SetPresent(Page->Get<UWidget>(TEXT("PlayerHeadRankBackground")), !Selected.Tag.IsEmpty());
	SetPageText(Page, TEXT("PlayerHeadPreviewRank"), FText::FromString(Selected.Tag));
	SetPageText(Page, TEXT("PlayerHeadPreviewName"), FText::FromString(Selected.Name));
	SetPageText(Page, TEXT("PlayerHeadPreviewDescription"), FText::FromString(Selected.Description));
	// No Role_SetRoleHead adapter exists: even free rows must not fabricate success.
	if (UButton* Confirm = Page->Get<UButton>(TEXT("PlayerHeadConfirmButton")))
	{
		Confirm->SetIsEnabled(false);
	}
	// Source side buttons are icon-only; the selected tab is the larger plate.
	for (int32 TabIndex = 0; TabIndex < 2; ++TabIndex)
	{
		const bool bSelectedTab = TabIndex == SelectedPlayerHeadPage;
		for (const TCHAR* Part : {TEXT("PlayerHeadTabBackground"), TEXT("PlayerHeadTabIcon")})
		{
			Page->SetShown(Page->Get<UWidget>(IndexedName(*(FString(Part) + TEXT("Selected")), TabIndex)), bSelectedTab);
			Page->SetShown(Page->Get<UWidget>(IndexedName(*(FString(Part) + TEXT("Dark")), TabIndex)), !bSelectedTab);
		}
		BindIndexedButton(Page->Get<UWidget>(IndexedName(TEXT("PlayerHeadPageButton"), TabIndex)), 3100 + TabIndex);
	}
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

	// Layout, section titles and row labels live in the page WBP.
	URokUiPageWidget* Page = MountModePage(TEXT("PlayerPowerDetails"));
	if (!Page)
	{
		return;
	}
	SetPageText(Page, TEXT("PlayerPowerPlayerName"), ProfileState.bHasDisplayName
		? FText::FromString(ProfileState.DisplayName)
		: NSLOCTEXT("RokModal", "PlayerPowerUnknownName", "城主名称未同步"));
	if (UButton* RenameButton = Page->Get<UButton>(TEXT("PlayerPowerRenameButton")))
	{
		RenameButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleProfileRenameClicked);
	}
	SetPageText(Page, TEXT("PlayerPowerLocalTotal"), FText::Format(
		NSLOCTEXT("RokModal", "PlayerPowerLocalFormat", "本地预览战力 {0}"),
		FText::AsNumber(ProfileState.LocalPreviewScore)));
	SetPageText(Page, TEXT("PlayerPowerKillCount"), ProfileState.bHasKillCount
		? FText::Format(NSLOCTEXT("RokModal", "PlayerPowerKillFormat", "击杀 {0}"), FText::AsNumber(ProfileState.KillCount))
		: NSLOCTEXT("RokModal", "PlayerPowerKillUnknown", "击杀 —"));
	// Only part of the composition is known locally (building upgrades, troops,
	// commanders); technology and the battle/resource statistics stay "—" in the
	// page until the server sends them.
	SetPageText(Page, TEXT("PlayerPowerCompositionRowValue_0"), FText::AsNumber(ProfileState.LocalBuildingUpgradeScore));
	SetPageText(Page, TEXT("PlayerPowerCompositionRowValue_2"), FText::AsNumber(ProfileState.LocalTroopPower));
	SetPageText(Page, TEXT("PlayerPowerCompositionRowValue_3"), FText::AsNumber(ProfileState.LocalHeroScore));
}

void URokUnityModalWidget::BuildPlayerRanking()
{
	FString Error;
	if (!RokLeaderboardCatalog::Load(PlayerRankingRows, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		const FName Name(TEXT("PlayerRankingCatalogMissing"));
		const FText Message = NSLOCTEXT("RokModal", "LeaderboardConfigMissing", "排行榜配置缺失");
		if (!TryMountStatusMessage(ModeCanvas, Name, TEXT("PlayerRankingCatalogError"), Message))
		{
			auto* Missing = MakeModalText(Name, 20, FLinearColor::Red);
			Missing->SetText(Message);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(400, 350), FVector2D(640, 50));
		}
		return;
	}
	// Source overview is gated by Rank_ShowRankFirst.response. This offline
	// surface is explicitly a configuration preview; it does not claim a response.

	if (SelectedRankingCategory == INDEX_NONE)
	{
		using namespace RokLeaderboardOverviewLayout;
		// The overview page holds the (empty) source pl_Total canvas and the boundary
		// line; each category is a tile WBP at its source grid position.
		URokUiPageWidget* Page = MountModePage(TEXT("PlayerRankingOverview"));
		UCanvasPanel* Overview = Page ? Page->Get<UCanvasPanel>(TEXT("PlayerRankingMainPanel")) : nullptr;
		for (int32 Index = 0; Overview && Index < PlayerRankingRows.Num(); ++Index)
		{
			const FRokLeaderboardRow& Row = PlayerRankingRows[Index];
			const FString Suffix = FString::Printf(TEXT("_%d"), Index);
			URokUiPageWidget* Tile = MountItem(TEXT("PlayerRankingTile"), Overview, *(TEXT("PlayerRankingCategory") + Suffix),
				TilePosition(Index), FVector2D(340.0f, 144.0f), 1, TEXT("PlayerRanking"), Suffix);
			if (!Tile)
			{
				break;
			}
			UImage* Icon = Tile->Get<UImage>(TEXT("CategoryIcon"));
			if (Row.HasIcon())
			{
				SetAtlasSprite(Icon, PlayerUiAtlasPath, Row.IconRect, PlayerUiAtlasSize, IconSize());
			}
			Tile->SetPresent(Icon, Row.HasIcon());
			// RefreshEntryBtn(null rankInfo): score and owner stay the crossbar the tile shows.
			SetPageText(Tile, TEXT("CategoryLabel"), FText::FromString(Row.Name));
			BindIndexedButton(Tile->Get<UWidget>(TEXT("CategoryButton")), 3200 + Index);
		}
		return;
	}

	const int32 CategoryIndex = FMath::Clamp(SelectedRankingCategory, 0, PlayerRankingRows.Num() - 1);
	const FRokLeaderboardRow& Row = PlayerRankingRows[CategoryIndex];
	// Panel, back button, column header, empty and boundary lines live in the page.
	URokUiPageWidget* Page = MountModePage(TEXT("PlayerRankingDetail"));
	if (!Page)
	{
		return;
	}
	BindIndexedButton(Page->Get<UWidget>(TEXT("PlayerRankingBackButton")), 3299);
	UImage* Icon = Page->Get<UImage>(TEXT("PlayerRankingDetailIcon"));
	if (Row.HasIcon())
	{
		SetAtlasSprite(Icon, PlayerUiAtlasPath, Row.IconRect, PlayerUiAtlasSize, FVector2D(76.0f, 90.0f));
	}
	Page->SetPresent(Icon, Row.HasIcon());
	SetPageText(Page, TEXT("PlayerRankingDetailTitle"), FText::FromString(Row.Name));
	// The value column is named by the leaderboard's first column.
	SetPageText(Page, TEXT("PlayerRankingColumn_2"),
		FText::FromString(Row.Columns.IsValidIndex(0) ? Row.Columns[0] : FString()));

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok player ranking screen: leaderboard_id=%d config_preview=1 authority=server_rows_not_synced."),
		Row.Id);
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

	// Background, back button, title, the flag detail group and the list
	// backdrop live in the page WBP, with CreateCharView's Expand anchors
	// (background stretched, list at the bottom, details centred). Each country
	// of the bottom list is a flag WBP; the selected one has its frame and pulse.
	URokUiPageWidget* Page = MountModePage(TEXT("CivilizationSelection"));
	if (!Page)
	{
		return;
	}
	const int32 BackgroundIndex = SelectedCivilization
		? FindCivilizationBackgroundIndex(SelectedCivilization->Id)
		: INDEX_NONE;
	UImage* Background = Page->Get<UImage>(TEXT("CivilizationSelectionBackground"));
	if (BackgroundIndex != INDEX_NONE)
	{
		SetAtlasSprite(Background, CivilizationBackgroundAtlasPath, CivilizationBackgroundRects[BackgroundIndex],
			CivilizationBackgroundAtlasSize, FVector2D(1440.0f, 810.0f));
	}
	Page->SetPresent(Background, BackgroundIndex != INDEX_NONE);

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
		// The animated mesh is runtime content: it goes into the page between the
		// background (z 0) and the flag detail group (z 2), centre-anchored.
		UCanvasPanel* PageCanvas = Cast<UCanvasPanel>(Page->GetRootWidget());
		if (CivilizationHero && PageCanvas)
		{
			CivilizationHero->SetCivilizationProjection(SelectedCivilization->HeroAboutOffset);
			PlaceOnCanvas(PageCanvas, CivilizationHero, FVector2D(-720.0f, -405.0f), FVector2D(1440, 810), 1);
			CastChecked<UCanvasPanelSlot>(CivilizationHero->Slot)->SetAnchors(FAnchors(0.5f, 0.5f));
		}
	}
	if (UButton* BackButton = Page->Get<UButton>(TEXT("CivilizationSelectionBackButton")))
	{
		BackButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleCloseClicked);
	}

	// The flag's descendants share its CanvasGroup and motion; country
	// selection, page back button and background are outside this group.
	CivilizationDetailRoot = Page->Get<UCanvasPanel>(TEXT("CivilizationDetailRoot"));
	const int32 VisualIndex = SelectedCivilization ? FindCivilizationVisualIndex(SelectedCivilization->Id) : INDEX_NONE;
	const int32 PreviewIndex = SelectedCivilization ? FindCivilizationUnlockedPreviewIndex(SelectedCivilization->Id) : INDEX_NONE;
	UImage* CityPreview = Page->Get<UImage>(TEXT("CivilizationCityPreview"));
	UImage* SoldierPreview = Page->Get<UImage>(TEXT("CivilizationSoldierPreview"));
	UImage* SelectedMark = Page->Get<UImage>(TEXT("CivilizationSelectedMark"));
	if (PreviewIndex != INDEX_NONE)
	{
		SetAtlasSprite(CityPreview, CivilizationImageAtlasPath, CivilizationSceneRects[PreviewIndex],
			CivilizationImageAtlasSize, FVector2D(734.0f, 261.0f));
		SetAtlasSprite(SoldierPreview, CivilizationImageAtlasPath, CivilizationSoldierRects[PreviewIndex],
			CivilizationImageAtlasSize, FVector2D(206.0f, 174.0f));
	}
	Page->SetPresent(CityPreview, PreviewIndex != INDEX_NONE);
	Page->SetPresent(SoldierPreview, PreviewIndex != INDEX_NONE);
	if (VisualIndex != INDEX_NONE)
	{
		SetAtlasSprite(SelectedMark, CivilizationAtlasPath, CivilizationMarkRects[VisualIndex],
			CivilizationAtlasSize, FVector2D(53.0f, 51.0f));
	}
	Page->SetPresent(SelectedMark, VisualIndex != INDEX_NONE);
	for (const TCHAR* Detail : {TEXT("CivilizationSelectedName"), TEXT("CivilizationBonusTitle"),
		TEXT("CivilizationBonusDescription"), TEXT("CivilizationInitialHero"), TEXT("CivilizationContinueButton")})
	{
		Page->SetPresent(Page->Get<UWidget>(Detail), SelectedCivilization != nullptr);
	}
	if (SelectedCivilization)
	{
		SetPageText(Page, TEXT("CivilizationSelectedName"), FText::FromString(SelectedCivilization->DisplayNameZhCn));
		SetPageText(Page, TEXT("CivilizationBonusTitle"), FText::FromString(SelectedCivilization->BonusNameZhCn));
		SetPageText(Page, TEXT("CivilizationBonusDescription"),
			FText::FromString(FormatCivilizationBonusDescription(*SelectedCivilization)));
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
		SetPageText(Page, TEXT("CivilizationInitialHero"), FText::Format(
			NSLOCTEXT("RokModal", "CivilizationInitialHeroFormat", "初始统帅：{0}"),
			InitialHeroName.IsEmpty()
				? FText::FromString(FString::FromInt(SelectedCivilization->InitialHeroId))
				: FText::FromString(InitialHeroName)));

		const bool bSameAsCurrent = ProfileState.bHasCivilization
			&& ProfileState.CivilizationId == SelectedCivilization->Id;
		SetPageText(Page, TEXT("CivilizationContinueButtonLabel"), bSameAsCurrent
			? NSLOCTEXT("RokModal", "CivilizationCurrent", "当前文明")
			: NSLOCTEXT("RokModal", "CivilizationChange", "更换文明"));
		if (UButton* ContinueButton = Page->Get<UButton>(TEXT("CivilizationContinueButton")))
		{
			ContinueButton->SetIsEnabled(!SelectedCivilization->bLocked && !bSameAsCurrent);
			if (ContinueButton->GetIsEnabled())
			{
				ContinueButton->OnClicked.AddDynamic(
					this, &URokUnityModalWidget::HandleCivilizationContinueClicked);
			}
		}
	}

	// Only sv_list_country descendants belong to the list animation. Keep
	// background, details, close and confirmation controls stationary.
	CivilizationListRoot = Page->Get<UCanvasPanel>(TEXT("CivilizationListRoot"));
	CivilizationSelectionPulse = nullptr;
	for (int32 Index = 0; CivilizationListRoot && Index < Civilizations.Num() && Index < 7; ++Index)
	{
		const FRokUnityCatalogCivilization& Civilization = Civilizations[Index];
		const int32 FlagIndex = FindCivilizationVisualIndex(Civilization.Id);
		if (FlagIndex == INDEX_NONE)
		{
			continue;
		}
		// CreateCharFlagItem: 199-unit pitch; the selected flag rises 28 units
		// inside its frame and its label turns dark.
		const float ItemX = 1241.0f - Index * 199.0f;
		const bool bSelected = Civilization.Id == SelectedCivilizationId;
		const FString Suffix = FString::Printf(TEXT("_%d"), Civilization.Id);
		URokUiPageWidget* Item = bSelected
			? MountItem(TEXT("CivilizationFlagSelected"), CivilizationListRoot, *(TEXT("CivilizationFlagItem") + Suffix),
				FVector2D(ItemX - 4.5f, 616.85f), FVector2D(208.0f, 204.0f), 5, TEXT("Civilization"), Suffix)
			: MountItem(TEXT("CivilizationFlag"), CivilizationListRoot, *(TEXT("CivilizationFlagItem") + Suffix),
				FVector2D(ItemX + 14.5f, 662.85f), FVector2D(170.0f, 150.0f), 5, TEXT("Civilization"), Suffix);
		if (!Item)
		{
			break;
		}
		SetAtlasSprite(Item->Get<UImage>(TEXT("Flag")), CivilizationAtlasPath, CivilizationFlagRects[FlagIndex],
			CivilizationAtlasSize, FVector2D(170.0f, 140.0f));
		SetPageText(Item, TEXT("ChoiceName"), FText::FromString(Civilization.DisplayNameZhCn));
		if (bSelected)
		{
			CivilizationSelectionPulse = Item->Get<UImage>(TEXT("SelectionPulse"));
			if (CivilizationSelectionPulse)
			{
				CivilizationSelectionPulse->SetRenderTransformPivot(FVector2D(0.5, 0));
			}
		}
		else
		{
			Item->SetPresent(Item->Get<UWidget>(TEXT("LockedShade")), Civilization.bLocked);
			Item->SetPresent(Item->Get<UWidget>(TEXT("LockedState")), Civilization.bLocked);
		}
		UWidget* Choice = Item->Get<UWidget>(TEXT("ChoiceButton"));
		if (Civilization.bLocked)
		{
			Item->SetPresent(Choice, false);
		}
		else
		{
			BindIndexedButton(Choice, 3300 + Index);
		}
	}

	SetPageText(Page, TEXT("CivilizationSelectionServerBoundaryState"), Civilizations.Num() == 7
		? NSLOCTEXT("RokModal", "CivilizationSelectionHint", "选择文明并查看加成 · 最终更换需要服务器确认")
		: NSLOCTEXT("RokModal", "CivilizationCatalogUnavailable", "文明配置尚未载入"));
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

	// The scroll, warning, the five server checks and the submit button live in
	// the page WBP; the civilization's name and bonus come from the catalog.
	URokUiPageWidget* Page = MountModePage(TEXT("CivilizationConfirmation"));
	if (!Page)
	{
		return;
	}
	SetPageText(Page, TEXT("CivilizationConfirmationName"), bFoundCivilization
		? FText::FromString(Civilization.DisplayNameZhCn)
		: NSLOCTEXT("RokModal", "CivilizationConfirmationMissing", "文明配置不可用"));
	SetPageText(Page, TEXT("CivilizationConfirmationBonus"), bFoundCivilization
		? FText::FromString(FormatCivilizationBonusDescription(Civilization))
		: FText::GetEmpty());
	// Role_ChangeCivilization is not wired: the submit button never enables.
	if (UButton* Submit = Page->Get<UButton>(TEXT("CivilizationSubmitButton")))
	{
		Submit->SetIsEnabled(false);
	}
}

void URokUnityModalWidget::BuildChangeName()
{
	// Layout, input style and the fixed texts live in the page WBP; validation
	// state is applied by UpdateChangeNameValidationState.
	URokUiPageWidget* Page = MountModePage(TEXT("ChangeName"));
	if (!Page)
	{
		return;
	}
	SetPageText(Page, TEXT("ChangeNameCurrentName"), bChangeNameHasCurrentName
		? FText::Format(
			NSLOCTEXT("RokModal", "ChangeNameCurrentNameValue", "当前名称：{0}"),
			FText::FromString(ChangeNameCurrentName))
		: NSLOCTEXT(
			"RokModal",
			"ChangeNameCurrentNameUnavailable",
			"当前名称：--（本地资料未载入）"));
	ChangeNameInput = Page->Get<UEditableTextBox>(TEXT("ChangeNameInput"));
	if (ChangeNameInput)
	{
		ChangeNameInput->SetText(FText::FromString(ChangeNameDraft));
		ChangeNameInput->OnTextChanged.AddDynamic(this, &URokUnityModalWidget::HandleChangeNameTextChanged);
	}
	ChangeNameCharacterCount = Page->Get<UTextBlock>(TEXT("ChangeNameCharacterCount"));
	SetPageText(Page, TEXT("ChangeNameCostDescription"), FText::Format(
		NSLOCTEXT("RokModal", "ChangeNameCostDescriptionText", "优先消耗改名许可 ×1；没有时消耗 {0} 宝石"),
		FText::AsNumber(ChangeNameGemCost)));
	ChangeNameValidationFeedback = Page->Get<UTextBlock>(TEXT("ChangeNameValidationFeedback"));
	ChangeNameServiceBoundaryState = Page->Get<UTextBlock>(TEXT("ChangeNameServiceBoundaryState"));
	ChangeNameConfirmButton = Page->Get<UButton>(TEXT("ChangeNameConfirmButton"));
	if (ChangeNameConfirmButton)
	{
		ChangeNameConfirmButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleChangeNameConfirmClicked);
	}
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
	// Layout and style live in the page WBP (one row per Unity item slot, a
	// selected and a dark look per tab). Here: which state shows, and the data.
	URokUiPageWidget* Page = MountModePage(TEXT("ResourceInventory"));
	if (!Page)
	{
		return;
	}

	static const int32 Amounts[4][4] = {
		{10000, 50000, 150000, 1000},
		{10000, 50000, 150000, 1000},
		{7500, 37500, 112500, 750},
		{3000, 15000, 50000, 500},
	};
	static const FText ResourceNames[4] = {
		NSLOCTEXT("RokModal", "Food", "食物"),
		NSLOCTEXT("RokModal", "Wood", "木材"),
		NSLOCTEXT("RokModal", "Stone", "石料"),
		NSLOCTEXT("RokModal", "Gold", "金币"),
	};

	const int32 Category = FMath::Clamp(SelectedResourceCategory, 0, 3);
	const FText& ResourceName = ResourceNames[Category];
	// Reference rows: quality, owned count and use/buy per slot are fixed in the
	// page; the item, amount and texts follow the selected category. Production
	// has no server item inventory yet, so it shows the empty state instead of
	// fabricated owned counts.
	const bool bReference = bReferenceResourceCaptureState;
	int32 RowCount = 0;
	for (int32 RowIndex = 0; RowIndex < 4; ++RowIndex)
	{
		Page->SetShown(Page->Get<UWidget>(IndexedName(TEXT("ResourceRow"), RowIndex)), bReference);
		if (!bReference)
		{
			continue;
		}
		++RowCount;
		const FText Amount = FormatNumber(Amounts[Category][RowIndex]);
		SetAtlasSprite(
			Page->Get<UImage>(IndexedName(TEXT("ResourceItemIcon"), RowIndex)),
			Item1AtlasPath,
			ItemIconRects[Category],
			Item1AtlasSize,
			FVector2D(120.0f));
		SetPageText(Page, IndexedName(TEXT("ResourceAmountBadge"), RowIndex), Amount);
		SetPageText(Page, IndexedName(TEXT("ResourceName"), RowIndex),
			FText::Format(NSLOCTEXT("RokModal", "ResourceItemName", "{0} {1}"), Amount, ResourceName));
		SetPageText(Page, IndexedName(TEXT("ResourceDescription"), RowIndex),
			FText::Format(NSLOCTEXT("RokModal", "ResourceDescription", "使用后获得{0}{1}"), Amount, ResourceName));
		for (const TCHAR* ButtonBase : {TEXT("ResourceUseButton"), TEXT("ResourceBuyButton")})
		{
			if (UButton* Button = Cast<UButton>(RokUiFind::Find(Page, IndexedName(ButtonBase, RowIndex))))
			{
				Button->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandlePreviewItemAction);
			}
		}
	}
	// Owned counts would come from the server item inventory; reference rows stay
	// labeled so they do not read as account state.
	Page->SetShown(Page->Get<UWidget>(TEXT("ResourceInventoryReferenceTag")), bReference);
	Page->SetShown(Page->Get<UWidget>(TEXT("ResourceInventoryEmptyState")), !bReference);
	Page->SetShown(Page->Get<UWidget>(TEXT("ResourceInventoryServiceState")), !bReference);

	for (int32 CategoryIndex = 0; CategoryIndex < 4; ++CategoryIndex)
	{
		const bool bSelected = CategoryIndex == Category;
		Page->SetShown(Page->Get<UWidget>(IndexedName(TEXT("ResourceTabSelected"), CategoryIndex)), bSelected);
		Page->SetShown(Page->Get<UWidget>(IndexedName(TEXT("ResourceTabIconSelected"), CategoryIndex)), bSelected);
		Page->SetShown(Page->Get<UWidget>(IndexedName(TEXT("ResourceTabDark"), CategoryIndex)), !bSelected);
		Page->SetShown(Page->Get<UWidget>(IndexedName(TEXT("ResourceTabIconDark"), CategoryIndex)), !bSelected);
		UButton* Tab = Page->Get<UButton>(IndexedName(TEXT("ResourceTab"), CategoryIndex));
		if (!Tab)
		{
			continue;
		}
		switch (CategoryIndex)
		{
		case 0: Tab->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleResourceTabFood); break;
		case 1: Tab->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleResourceTabWood); break;
		case 2: Tab->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleResourceTabStone); break;
		case 3: Tab->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleResourceTabGold); break;
		default: break;
		}
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok resource inventory: category=%d reference=%d rows=%d authority=%s."),
		SelectedResourceCategory,
		bReference ? 1 : 0,
		RowCount,
		bReference ? TEXT("local_fixture") : TEXT("server"));
}

void URokUnityModalWidget::BuildWarWarning()
{
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok war warning screen: reference=%d authority=%s."),
		bReferenceWarWarningCaptureState ? 1 : 0,
		bReferenceWarWarningCaptureState ? TEXT("local_fixture") : TEXT("server"));
	// Both states are fixed layouts in the page WBP: the service boundary (no
	// Role_EarlyWarningInfo data yet) and the labeled Unity reference fixture.
	URokUiPageWidget* Page = MountModePage(TEXT("WarWarning"));
	if (!Page)
	{
		return;
	}
	static const TCHAR* BoundaryWidgets[] = {
		TEXT("WarWarningServiceBoundaryPanel"),
		TEXT("WarWarningServiceBoundaryTitle"),
		TEXT("WarWarningServiceBoundaryState"),
		TEXT("WarWarningServerActionsDisabled"),
	};
	static const TCHAR* ReferenceWidgets[] = {
		TEXT("WarWarningReferencePreviewNotice"),
		TEXT("WarWarningReferenceIgnoreAllDisabled"),
		TEXT("WarWarningRow_0"),
		TEXT("WarWarningRow_1"),
		TEXT("WarWarningRow_2"),
		TEXT("WarWarningRow_3"),
		TEXT("WarWarningDetailStructure"),
		TEXT("WarWarningDetailTitle"),
		TEXT("WarWarningDetailBody"),
	};
	for (const TCHAR* Name : BoundaryWidgets)
	{
		Page->SetShown(Page->Get<UWidget>(Name), !bReferenceWarWarningCaptureState);
	}
	for (const TCHAR* Name : ReferenceWidgets)
	{
		Page->SetShown(Page->Get<UWidget>(Name), bReferenceWarWarningCaptureState);
	}
}

void URokUnityModalWidget::BuildBuffList()
{
	// UI_Pop_BuffList.prefab. The page WBP holds both states: the service
	// boundary and the static-configuration preview with up to two definition
	// rows. BuffListAnimationRoot (pl_pos) is what the open animation scales.
	URokUiPageWidget* Page = MountModePage(TEXT("BuffList"));
	if (!Page)
	{
		return;
	}
	BuffListAnimationRoot = Page->Get<UCanvasPanel>(TEXT("BuffListAnimationRoot"));
	if (UButton* MoreButton = Page->Get<UButton>(TEXT("BuffListMoreButton")))
	{
		MoreButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleBuffListMoreClicked);
	}
	static const TCHAR* BoundaryWidgets[] = {
		TEXT("BuffListServiceBoundaryPanel"),
		TEXT("BuffListServiceBoundaryTitle"),
		TEXT("BuffListServiceBoundaryState"),
		TEXT("BuffListCityManagerNavigationHint"),
	};
	for (const TCHAR* Name : BoundaryWidgets)
	{
		Page->SetShown(Page->Get<UWidget>(Name), !bReferenceBuffListCaptureState);
	}
	Page->SetShown(Page->Get<UWidget>(TEXT("BuffListReferencePreviewNotice")), bReferenceBuffListCaptureState);

	constexpr int32 MaxRows = 2;
	int32 ShownCount = 0;
	if (bReferenceBuffListCaptureState)
	{
		for (const FCityBuffPreviewRow& Row : LoadCityBuffPreviewRows(MaxRows))
		{
			const int32 RowIndex = ShownCount++;
			Page->SetShown(Page->Get<UWidget>(IndexedName(TEXT("BuffListDefinitionRow"), RowIndex)), true);
			SetAtlasSprite(Page->Get<UImage>(IndexedName(TEXT("BuffListDefinitionIcon"), RowIndex)),
				MainMenuAtlasPath, Row.IconRect, MainMenuAtlasSize, FVector2D(54.0f, 54.0f));
			SetCityBuffPreviewRowText(Page, TEXT("BuffListDefinition"), RowIndex, Row);
		}
		if (ShownCount == 0 && BuffListAnimationRoot)
		{
			const FName Name(TEXT("BuffListDefinitionUnavailable"));
			const FText Message = NSLOCTEXT("RokModal", "BuffListDefinitionUnavailable", "增益静态配置未发布或不可读取");
			if (!TryMountStatusMessage(BuffListAnimationRoot, Name, TEXT("BuffDefinitionError"), Message))
			{
				auto* Missing = MakeModalText(Name, 16, FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
				Missing->SetText(Message);
				PlaceOnCanvas(BuffListAnimationRoot, Missing, FVector2D(42.0f, 132.0f), FVector2D(416.0f, 60.0f), 3);
			}
		}
	}
	for (int32 RowIndex = ShownCount; RowIndex < MaxRows; ++RowIndex)
	{
		Page->SetShown(Page->Get<UWidget>(IndexedName(TEXT("BuffListDefinitionRow"), RowIndex)), false);
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok buff list screen: reference=%d authority=%s."),
		bReferenceBuffListCaptureState ? 1 : 0,
		bReferenceBuffListCaptureState ? TEXT("local_fixture") : TEXT("server"));
}

void URokUnityModalWidget::BuildGainEffect()
{
	// UI_Win_GainEffect.prefab: the reference list (up to four economy rows, or
	// the military notice) and the vertical economy/military tabs, laid out in
	// the page WBP. The selected tab's tint is state, so it is applied here.
	URokUiPageWidget* Page = MountModePage(TEXT("GainEffect"));
	if (!Page)
	{
		return;
	}
	GainEffectListRoot = Page->Get<UCanvasPanel>(TEXT("GainEffectListRoot"));
	for (int32 Tab = 0; Tab < 2; ++Tab)
	{
		const bool bSelected = Tab == GainEffectTabIndex;
		if (UButton* TabButton = Page->Get<UButton>(IndexedName(TEXT("GainEffectTab"), Tab)))
		{
			if (Tab == 0)
			{
				TabButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleGainEffectEconomyTab);
			}
			else
			{
				TabButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleGainEffectMilitaryTab);
			}
		}
		if (UTextBlock* Label = Page->Get<UTextBlock>(IndexedName(TEXT("GainEffectTabLabel"), Tab)))
		{
			Label->SetColorAndOpacity(FSlateColor(bSelected
				? FLinearColor(1.0f, 0.86f, 0.42f, 1.0f)
				: FLinearColor(0.46f, 0.40f, 0.31f, 1.0f)));
		}
		if (UImage* Plate = Page->Get<UImage>(IndexedName(TEXT("GainEffectTabPlate"), Tab)))
		{
			Plate->SetColorAndOpacity(bSelected
				? FLinearColor(1.0f, 0.92f, 0.68f, 1.0f)
				: FLinearColor(0.72f, 0.70f, 0.66f, 1.0f));
		}
	}
	RebuildGainEffectContent();
}

void URokUnityModalWidget::RebuildGainEffectContent()
{
	URokUiPageWidget* Page = ActivePage;
	if (!Page || !GainEffectListRoot)
	{
		return;
	}
	// No military fixture exists: CityBuff has no military reference rows and
	// the active state needs the server CityBuff snapshot.
	const bool bMilitary = GainEffectTabIndex == 1;
	Page->SetShown(Page->Get<UWidget>(TEXT("GainEffectMilitaryNotice")), bMilitary);
	constexpr int32 MaxRows = 4;
	const TArray<FCityBuffPreviewRow> Rows = bMilitary ? TArray<FCityBuffPreviewRow>() : LoadCityBuffPreviewRows(MaxRows);
	const int32 Slots = CountPageSlots(Page, TEXT("GainEffectRow"), Rows.Num());
	for (int32 RowIndex = 0; RowIndex < Slots; ++RowIndex)
	{
		const bool bShown = Rows.IsValidIndex(RowIndex);
		Page->SetShown(Page->Get<UWidget>(IndexedName(TEXT("GainEffectRow"), RowIndex)), bShown);
		if (bShown)
		{
			SetAtlasSprite(Page->Get<UImage>(IndexedName(TEXT("GainEffectIcon"), RowIndex)),
				MainMenuAtlasPath, Rows[RowIndex].IconRect, MainMenuAtlasSize, FVector2D(54.0f, 54.0f));
			SetCityBuffPreviewRowText(Page, TEXT("GainEffect"), RowIndex, Rows[RowIndex]);
		}
	}
	if (!bMilitary && Rows.IsEmpty())
	{
		// Only when the published configuration is missing; not part of the page.
		UTextBlock* MissingDefinitions = MakeModalText(
			TEXT("GainEffectDefinitionsUnavailable"), 16,
			FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
		MissingDefinitions->SetText(NSLOCTEXT(
			"RokModal", "GainEffectDefinitionsUnavailable", "增益静态配置未发布或不可读取"));
		PlaceOnCanvas(GainEffectListRoot, MissingDefinitions, FVector2D(20.0f, 120.0f), FVector2D(660.0f, 60.0f), 2);
	}
}

void URokUnityModalWidget::HandleGainEffectEconomyTab()
{
	if (ActiveMode != ERokUnityModalMode::GainEffect || GainEffectTabIndex == 0)
	{
		return;
	}
	GainEffectTabIndex = 0;
	RebuildModeContent(false);
}

void URokUnityModalWidget::HandleGainEffectMilitaryTab()
{
	if (ActiveMode != ERokUnityModalMode::GainEffect || GainEffectTabIndex == 1)
	{
		return;
	}
	GainEffectTabIndex = 1;
	RebuildModeContent(false);
}

namespace
{
	// Unity config CSVs quote fields that contain commas (e.g. reward "id,count"),
	// so plain splitting shifts columns. RFC 4180 subset: doubled quotes escape.
	void SplitRokCsvLine(const FString& Line, TArray<FString>& OutFields)
	{
		OutFields.Reset();
		FString Current;
		bool bInQuotes = false;
		for (int32 Index = 0; Index < Line.Len(); ++Index)
		{
			const TCHAR Character = Line[Index];
			if (bInQuotes)
			{
				if (Character == TEXT('"'))
				{
					if (Index + 1 < Line.Len() && Line[Index + 1] == TEXT('"'))
					{
						Current.AppendChar(TEXT('"'));
						++Index;
					}
					else
					{
						bInQuotes = false;
					}
				}
				else
				{
					Current.AppendChar(Character);
				}
			}
			else if (Character == TEXT('"'))
			{
				bInQuotes = true;
			}
			else if (Character == TEXT(','))
			{
				OutFields.Add(Current);
				Current.Reset();
			}
			else
			{
				Current.AppendChar(Character);
			}
		}
		OutFields.Add(Current);
	}
}

void URokUnityModalWidget::BuildBattlePassRewards()
{
	// UI_Win_BattlePassRewards.prefab: 1440x810 window, `pl_mes` 910x678 with
	// `sv_list` 1101x548. Rows come from BattlePassReward.csv (age 0, level row)
	// and item names resolve through Item.csv -> Language.csv.
	constexpr int32 MaxRows = 5;

	TMap<FString, FString> ItemNames;
	FString ItemCsv;
	if (FFileHelper::LoadFileToString(ItemCsv, *RokDataPaths::Resolve(TEXT("ConfigTables/Item.csv"))))
	{
		TArray<FString> Lines;
		ItemCsv.ParseIntoArrayLines(Lines, false);
		if (Lines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(Lines[0], Columns);
			int32 IdColumn = INDEX_NONE;
			int32 NameColumn = INDEX_NONE;
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("ID")) IdColumn = Column;
				else if (Name == TEXT("l_nameId")) NameColumn = Column;
			}
			if (IdColumn != INDEX_NONE && NameColumn != INDEX_NONE)
			{
				const int32 Required = FMath::Max(IdColumn, NameColumn);
				for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(Lines[LineIndex], Fields);
					if (Fields.Num() <= Required)
					{
						continue;
					}
					const FString Resolved = FRokUiTextTable::Resolve(FCString::Atoi(*Fields[NameColumn]));
					if (!Resolved.IsEmpty())
					{
						ItemNames.Add(Fields[IdColumn].TrimStartAndEnd(), Resolved);
					}
				}
			}
		}
	}

	auto DescribeReward = [&ItemNames](const FString& Reward) -> FString
	{
		// A track can list several pairs separated by '|', each "itemId,count".
		TArray<FString> Groups;
		Reward.ParseIntoArray(Groups, TEXT("|"), false);
		TArray<FString> Described;
		for (const FString& Group : Groups)
		{
			TArray<FString> Parts;
			Group.ParseIntoArray(Parts, TEXT(","), false);
			if (Parts.Num() < 2)
			{
				continue;
			}
			const int32 Count = FCString::Atoi(*Parts[1].TrimStartAndEnd());
			if (Count <= 0)
			{
				continue;
			}
			const FString ItemId = Parts[0].TrimStartAndEnd();
			const FString* Name = ItemNames.Find(ItemId);
			Described.Add(FString::Printf(TEXT("%s x%d"),
				Name ? **Name : *ItemId, Count));
		}
		return FString::Join(Described, TEXT("、"));
	};

	struct FBattlePassRow
	{
		int32 Level = 0;
		FString Free;
		FString Super;
	};
	TArray<FBattlePassRow> Rows;
	FString Csv;
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/BattlePassReward.csv")))
		&& !Csv.IsEmpty())
	{
		Csv.ParseIntoArrayLines(Lines, false);
		if (Lines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(Lines[0], Columns);
			int32 AgeColumn = INDEX_NONE;
			int32 LevelColumn = INDEX_NONE;
			int32 RewardColumn = INDEX_NONE;
			int32 SuperColumn = INDEX_NONE;
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("age")) AgeColumn = Column;
				else if (Name == TEXT("lv")) LevelColumn = Column;
				else if (Name == TEXT("reward")) RewardColumn = Column;
				else if (Name == TEXT("superRewards")) SuperColumn = Column;
			}
			const int32 Required = FMath::Max(FMath::Max(AgeColumn, LevelColumn),
				FMath::Max(RewardColumn, SuperColumn));
			if (Required != INDEX_NONE)
			{
				for (int32 LineIndex = 1; LineIndex < Lines.Num() && Rows.Num() < MaxRows; ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(Lines[LineIndex], Fields);
					if (Fields.Num() <= Required || FCString::Atoi(*Fields[AgeColumn]) != 0)
					{
						continue;
					}
					const int32 Level = FCString::Atoi(*Fields[LevelColumn]);
					if (Level <= 0)
					{
						continue;
					}
					FBattlePassRow Row;
					Row.Level = Level;
					Row.Free = DescribeReward(Fields[RewardColumn]);
					if (Row.Free.IsEmpty())
					{
						continue;
					}
					Row.Super = DescribeReward(Fields[SuperColumn]);
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	// Header, the five row plates and the note live in the page WBP.
	URokUiPageWidget* Page = MountModePage(TEXT("BattlePassRewards"));
	if (!Page)
	{
		return;
	}
	for (int32 RowIndex = 0; RowIndex < MaxRows; ++RowIndex)
	{
		Page->SetShown(Page->Get<UWidget>(IndexedName(TEXT("BattlePassRow"), RowIndex)), Rows.IsValidIndex(RowIndex));
		if (!Rows.IsValidIndex(RowIndex))
		{
			continue;
		}
		const FBattlePassRow& Row = Rows[RowIndex];
		SetPageText(Page, IndexedName(TEXT("BattlePassRowLevel"), RowIndex),
			FText::FromString(FString::Printf(TEXT("等级 %d"), Row.Level)));
		SetPageText(Page, IndexedName(TEXT("BattlePassRowFree"), RowIndex),
			FText::FromString(FString(TEXT("免费  ")) + Row.Free));
		SetPageText(Page, IndexedName(TEXT("BattlePassRowSuper"), RowIndex), FText::FromString(
			Row.Super.IsEmpty() ? FString(TEXT("进阶  —")) : FString(TEXT("进阶  ")) + Row.Super));
	}
	Page->SetShown(Page->Get<UWidget>(TEXT("BattlePassNote")), !Rows.IsEmpty());
	if (Rows.IsEmpty())
	{
		if (UCanvasPanel* PageCanvas = Cast<UCanvasPanel>(Page->GetRootWidget()))
		{
			const FText StatusMessage = NSLOCTEXT("RokModal", "BattlePassUnavailable",
				"战令奖励配置未发布或不可读取");
			if (!TryMountStatusMessage(PageCanvas, TEXT("BattlePassUnavailable"), TEXT("Wide"), StatusMessage))
			{
				UTextBlock* Missing = MakeModalText(TEXT("BattlePassUnavailable"), 16,
					FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
				Missing->SetText(StatusMessage);
				PlaceOnCanvas(PageCanvas, Missing, FVector2D(212.0f, 300.0f), FVector2D(760.0f, 60.0f), 3);
			}
		}
	}
}

void URokUnityModalWidget::BuildDefinitionList(
	const FText& Header,
	const TArray<FRokDefinitionRow>& Rows,
	const FText& Footer,
	const FString& WidgetPrefix,
	float RowWidth,
	float RowHeight,
	bool bWrapDetail,
	bool bWithSearch,
	int32 Columns)
{
	const float RowSpacing = RowHeight + 8.0f;
	ActiveListRows = Rows;
	ActiveListFilter.Reset();
	ActiveListPrefix = WidgetPrefix;
	ActiveRowWidth = RowWidth;
	ActiveRowHeight = RowHeight;
	bActiveRowsWrap = bWrapDetail;
	ActiveListColumns = FMath::Max(1, Columns);
	const int32 MaxRows = 200;

	const URokUiSettings* UiSettings = GetDefault<URokUiSettings>();
	// Three list WBPs exist: list, list + search, 3-column grid + search; all
	// laid out for 700-wide rows like every caller today. Anything else keeps the C++ build.
	UClass* ListClass = nullptr;
	if (RowWidth == 700.0f)
	{
		if (Columns <= 1)
		{
			ListClass = URokUiSettings::Resolve(bWithSearch ? UiSettings->DefinitionListWithSearch : UiSettings->DefinitionList);
		}
		else if (Columns == 3 && bWithSearch)
		{
			ListClass = URokUiSettings::Resolve(UiSettings->DefinitionGridWithSearch);
		}
	}
	if (ListClass)
	{
		URokDefinitionListWidget* List = CreateWidget<URokDefinitionListWidget>(this, ListClass);
		List->LegacyPrefix = WidgetPrefix;
		List->Header->SetText(Header);
		List->Footer->SetText(Footer);
		if (List->Search)
		{
			List->Search->OnTextChanged.AddUniqueDynamic(this, &URokUnityModalWidget::HandleDefinitionFilterChanged);
		}
		ActiveListCanvas = List->ScrollContent;
		ActiveListSize = List->ScrollSize;
		MountFullPiece(List, 1);
		PopulateDefinitionRows();
		return;
	}

	UTextBlock* HeaderText = MakeModalText(
		*FString::Printf(TEXT("%sHeader"), *WidgetPrefix), 18,
		FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), true);
	HeaderText->SetText(Header);
	PlaceOnCanvas(ModeCanvas, HeaderText, FVector2D(212.0f, Columns > 1 ? 166.0f : 204.0f),
		FVector2D(RowWidth, 28.0f), 3);

	if (bWithSearch)
	{
		UEditableTextBox* Search = WidgetTree->ConstructWidget<UEditableTextBox>(
			UEditableTextBox::StaticClass(), *FString::Printf(TEXT("%sSearch"), *WidgetPrefix));
		// The stock editable-text box is a light Windows-style field; a plate plus
		// dark tints keeps it inside the frame's look.
		PlaceOnCanvas(ModeCanvas, MakeAtlasBorder(
			TEXT("DefinitionSearchPlate"), CommonAtlasPath, ResourceRowRect, CommonAtlasSize,
			FVector2D(304.0f, 38.0f), ResourceRowMargin),
			FVector2D(210.0f, 234.0f), FVector2D(304.0f, 38.0f), 2);
		Search->SetHintText(NSLOCTEXT("RokModal", "DefinitionSearchHint", "搜索名称…"));
		Search->OnTextChanged.AddDynamic(this, &URokUnityModalWidget::HandleDefinitionFilterChanged);
		PlaceOnCanvas(ModeCanvas, Search, FVector2D(212.0f, Columns > 1 ? 196.0f : 236.0f),
			FVector2D(300.0f, 34.0f), 3);
	}

	const float ScrollTop = bWithSearch ? 280.0f : 238.0f;
	const float ScrollHeight = bWithSearch ? 452.0f : 496.0f;
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), *FString::Printf(TEXT("%sScroll"), *WidgetPrefix));
	Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	PlaceOnCanvas(ModeCanvas, Scroll, FVector2D(206.0f, ScrollTop),
		FVector2D(RowWidth + 12.0f, ScrollHeight), 3);

	ActiveListCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), *FString::Printf(TEXT("%sScrollContent"), *WidgetPrefix));
	ActiveListSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), *FString::Printf(TEXT("%sScrollSize"), *WidgetPrefix));
	ActiveListSize->SetWidthOverride(RowWidth + 12.0f);
	ActiveListSize->SetHeightOverride(FMath::Max(1, FMath::Min(Rows.Num(), MaxRows)) * RowSpacing);
	ActiveListSize->SetContent(ActiveListCanvas);
	Scroll->AddChild(ActiveListSize);

	PopulateDefinitionRows();

	UTextBlock* FooterText = MakeModalText(
		*FString::Printf(TEXT("%sFooter"), *WidgetPrefix), 14,
		FLinearColor(0.61f, 0.38f, 0.14f, 1.0f), false, ETextJustify::Center);
	FooterText->SetText(Footer);
	PlaceOnCanvas(ModeCanvas, FooterText, FVector2D(212.0f, 748.0f),
		FVector2D(RowWidth, 26.0f), 3);
}

void URokUnityModalWidget::PopulateDefinitionRows()
{
	if (!ActiveListCanvas)
	{
		return;
	}
	const TArray<UWidget*> OldRows = ActiveListCanvas->GetAllChildren();
	ActiveListCanvas->ClearChildren();
	for (UWidget* OldRow : OldRows)
	{
		// WBP rows are user widgets; free their names for the refilled rows.
		if (OldRow && OldRow->IsA<UUserWidget>())
		{
			RokUiFind::Retire(OldRow);
		}
	}
	const URokUiSettings* UiSettings = GetDefault<URokUiSettings>();
	const int32 MaxRows = 200;
	const int32 Columns = FMath::Max(1, ActiveListColumns);
	const float GapX = 8.0f;
	const float GapY = 8.0f;
	const float CardWidth = Columns > 1
		? (ActiveRowWidth - (Columns - 1) * GapX) / Columns
		: ActiveRowWidth;
	const float CardHeight = ActiveRowHeight;
	int32 Shown = 0;
	for (int32 Index = 0; Index < ActiveListRows.Num() && Shown < MaxRows; ++Index)
	{
		const FRokDefinitionRow& RowData = ActiveListRows[Index];
		bool bMatches = true;
		if (!ActiveListFilter.IsEmpty())
		{
			// `|` joins independent terms; every term must match title or detail.
			TArray<FString> Terms;
			ActiveListFilter.ParseIntoArray(Terms, TEXT("|"), true);
			for (const FString& Term : Terms)
			{
				const FString Trimmed = Term.TrimStartAndEnd();
				if (Trimmed.IsEmpty())
				{
					continue;
				}
				if (!RowData.Title.Contains(Trimmed, ESearchCase::IgnoreCase)
					&& !RowData.Detail.Contains(Trimmed, ESearchCase::IgnoreCase))
				{
					bMatches = false;
					break;
				}
			}
		}
		if (!bMatches)
		{
			continue;
		}
		const int32 Column = Columns > 1 ? Shown % Columns : 0;
		const int32 RowIndex = Columns > 1 ? Shown / Columns : Shown;
		const FVector2D Offset(
			Column * (CardWidth + GapX),
			RowIndex * (CardHeight + GapY));

		UTexture2D* IconSheet = nullptr;
		FBox2D IconUv(ForceInit);
		ResolveDefinitionIcon(RowData.Icon, IconSheet, IconUv);
		UClass* RowClass = nullptr;
		if (Columns <= 1)
		{
			RowClass = URokUiSettings::Resolve(IconSheet ? UiSettings->DefinitionRowWithIcon : UiSettings->DefinitionRow);
		}
		else if (IconSheet)
		{
			RowClass = URokUiSettings::Resolve(UiSettings->DefinitionCardWithIcon);
		}
		if (RowClass)
		{
			const FName RowName(*FString::Printf(TEXT("%sRow_%d"), *ActiveListPrefix, Shown));
			// A C++-built row of the same name may still sit in the same widget tree.
			if (UObject* Existing = StaticFindObjectFast(nullptr, ActiveListCanvas->GetOuter(), RowName))
			{
				RokUiFind::Retire(Existing);
			}
			URokDefinitionRowWidget* RowWidget = CreateWidget<URokDefinitionRowWidget>(ActiveListCanvas, RowClass, RowName);
			RowWidget->LegacyPrefix = ActiveListPrefix;
			RowWidget->LegacySuffix = FString::Printf(TEXT("_%d"), Shown);
			RowWidget->RowTitle->SetText(FText::FromString(RowData.Title));
			SetOptionalRowText(RowWidget->RowDetail, RowData.Detail);
			if (RowWidget->RowDetail && bActiveRowsWrap)
			{
				RowWidget->RowDetail->SetAutoWrapText(true);
			}
			SetOptionalRowText(RowWidget->RowTag, RowData.Tag);
			if (RowWidget->RowIcon && IconSheet)
			{
				FSlateBrush Brush = RowWidget->RowIcon->GetBrush();
				Brush.SetResourceObject(IconSheet);
				Brush.SetUVRegion(IconUv);
				RowWidget->RowIcon->SetBrush(Brush);
			}
			PlaceOnCanvas(ActiveListCanvas, RowWidget, Offset, FVector2D(CardWidth, CardHeight), 3);
			++Shown;
			continue;
		}

		UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(),
			*FString::Printf(TEXT("%sRow_%d"), *ActiveListPrefix, Shown));
		PlaceOnCanvas(Row, MakeAtlasBorder(
			*FString::Printf(TEXT("%sRowBackground_%d"), *ActiveListPrefix, Shown),
			CommonAtlasPath, ResourceRowRect, CommonAtlasSize,
			FVector2D(CardWidth, CardHeight), ResourceRowMargin),
			FVector2D::ZeroVector, FVector2D(CardWidth, CardHeight));

		// Icon column: resolved from the `.tpsheet` manifest, decoded once.
		float TextLeft = 12.0f;
		if (IconSheet)
		{
			UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
				*FString::Printf(TEXT("%sRowIcon_%d"), *ActiveListPrefix, Shown));
			FSlateBrush Brush;
			Brush.SetResourceObject(IconSheet);
			Brush.ImageSize = FVector2D(Columns > 1 ? 52.0f : 40.0f, Columns > 1 ? 52.0f : 40.0f);
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.SetUVRegion(IconUv);
			Icon->SetBrush(Brush);
			PlaceOnCanvas(Row, Icon, FVector2D(8.0f, 8.0f),
				FVector2D(Columns > 1 ? 52.0f : 40.0f, Columns > 1 ? 52.0f : 40.0f), 2);
			TextLeft = Columns > 1 ? 66.0f : 56.0f;
		}
		UTextBlock* Title = MakeModalText(
			*FString::Printf(TEXT("%sRowTitle_%d"), *ActiveListPrefix, Shown), Columns > 1 ? 14 : 18,
			FLinearColor(0.28f, 0.22f, 0.14f, 1.0f), true);
		Title->SetAutoWrapText(Columns > 1);
		Title->SetText(FText::FromString(RowData.Title));
		PlaceOnCanvas(Row, Title, FVector2D(TextLeft, 8.0f),
			FVector2D(CardWidth - TextLeft - 10.0f, Columns > 1 ? 48.0f : 26.0f), 2);
		if (!RowData.Detail.IsEmpty())
		{
			UTextBlock* Detail = MakeModalText(
				*FString::Printf(TEXT("%sRowDetail_%d"), *ActiveListPrefix, Shown), 13,
				FLinearColor(0.50f, 0.43f, 0.34f, 1.0f));
			Detail->SetText(FText::FromString(RowData.Detail));
			Detail->SetAutoWrapText(bActiveRowsWrap);
			PlaceOnCanvas(Row, Detail, FVector2D(TextLeft, Columns > 1 ? 50.0f : 38.0f),
				FVector2D(CardWidth - TextLeft - 10.0f, 22.0f), 2);
		}
		if (!RowData.Tag.IsEmpty())
		{
			UTextBlock* Tag = MakeModalText(
				*FString::Printf(TEXT("%sRowTag_%d"), *ActiveListPrefix, Shown), 14,
				FLinearColor(0.61f, 0.38f, 0.14f, 1.0f), true,
				Columns > 1 ? ETextJustify::Left : ETextJustify::Right);
			Tag->SetText(FText::FromString(RowData.Tag));
			PlaceOnCanvas(Row, Tag,
				Columns > 1 ? FVector2D(12.0f, CardHeight - 26.0f) : FVector2D(CardWidth - 150.0f, 24.0f),
				Columns > 1 ? FVector2D(CardWidth - 24.0f, 22.0f) : FVector2D(132.0f, 24.0f), 2);
		}
		PlaceOnCanvas(ActiveListCanvas, Row, Offset, FVector2D(CardWidth, CardHeight), 3);
		++Shown;
	}
	ActiveDefinitionRowCount = Shown;
	if (ActiveListSize)
	{
		const int32 LayoutRows = Columns > 1 ? FMath::Max(1, (Shown + Columns - 1) / Columns) : FMath::Max(1, Shown);
		ActiveListSize->SetHeightOverride(LayoutRows * (CardHeight + GapY));
	}
}


void URokUnityModalWidget::HandleDefinitionFilterChanged(const FText& Text)
{
	SetDefinitionFilter(Text.ToString());
}

void URokUnityModalWidget::SetDefinitionFilter(const FString& Filter)
{
	ActiveListFilter = Filter.TrimStartAndEnd();
	PopulateDefinitionRows();
	// The compendium footer reports the active filter and the rendered count.
	if (ActiveListPrefix == TEXT("ItemCollection"))
	{
		if (UTextBlock* Footer = Cast<UTextBlock>(RokUiFind::Find(this, TEXT("ItemCollectionFooter"))))
		{
			Footer->SetText(FText::FromString(ActiveListFilter.IsEmpty()
				? FString::Printf(TEXT("共 %d 件（显示前 200）· 持有与使用需服务器背包"), ItemCollectionTotal)
				: FString::Printf(TEXT("共 %d 件 · 当前筛选：%s · 显示 %d 件"),
					ItemCollectionTotal, *ActiveListFilter, GetDefinitionRowCount())));
		}
	}
}

void URokUnityModalWidget::BuildEventDate()
{
	// UI_Win_EventDate.prefab: calendar window. Rows come from ActivityCalendar.csv
	// (l_nameID, startTime "HH|MM|SS", durationTime seconds).
	TArray<FRokDefinitionRow> Rows;
	FString Csv;
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/ActivityCalendar.csv")))
		&& !Csv.IsEmpty())
	{
		Csv.ParseIntoArrayLines(Lines, false);
		if (Lines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(Lines[0], Columns);
			int32 NameColumn = INDEX_NONE;
			int32 StartColumn = INDEX_NONE;
			int32 DurationColumn = INDEX_NONE;
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("l_nameId")) NameColumn = Column;
				else if (Name == TEXT("startTime")) StartColumn = Column;
				else if (Name == TEXT("durationTime")) DurationColumn = Column;
			}
			const int32 Required = FMath::Max(FMath::Max(NameColumn, StartColumn), DurationColumn);
			if (Required != INDEX_NONE)
			{
				TSet<FString> SeenNames;
				for (int32 LineIndex = 1; LineIndex < Lines.Num() && Rows.Num() < 200; ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(Lines[LineIndex], Fields);
					if (Fields.Num() <= Required)
					{
						continue;
					}
					const FString ActivityName = FRokUiTextTable::Resolve(FCString::Atoi(*Fields[NameColumn]));
					if (ActivityName.IsEmpty() || SeenNames.Contains(ActivityName))
					{
						continue;
					}
					const int32 Seconds = FCString::Atoi(*Fields[DurationColumn]);
					if (Seconds <= 0)
					{
						continue;
					}
					FString StartText = Fields[StartColumn].TrimStartAndEnd();
					StartText.ReplaceInline(TEXT("|"), TEXT(":"));
					SeenNames.Add(ActivityName);
					FRokDefinitionRow Row;
					Row.Title = ActivityName;
					Row.Detail = FString::Printf(TEXT("开始 %s · 持续 %d 小时"),
						*StartText, FMath::Max(1, FMath::RoundToInt(Seconds / 3600.0f)));
					Row.Tag = TEXT("活动");
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "EventDateUnavailable",
			"活动日程配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("EventDateUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("EventDateUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "EventDateHeader", "活动日程 · 本地配置预览（未开启）"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 条 · 开启时间以服务器为准"), Rows.Num())),
		TEXT("EventDate"));
}

void URokUnityModalWidget::BuildGuildPurview()
{
	// UI_Win_GuildPurview: one row per alliance permission; `R1..R5` columns in
	// AllianceMemberJurisdiction.csv say which ranks may use it.
	TArray<FRokDefinitionRow> Rows;
	FString Csv;
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/AllianceMemberJurisdiction.csv")))
		&& !Csv.IsEmpty())
	{
		Csv.ParseIntoArrayLines(Lines, false);
		if (Lines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(Lines[0], Columns);
			int32 NameColumn = INDEX_NONE;
			int32 RankColumns[5] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("l_typeID"))
				{
					NameColumn = Column;
				}
				for (int32 Rank = 0; Rank < 5; ++Rank)
				{
					if (Name == FString::Printf(TEXT("R%d"), Rank + 1))
					{
						RankColumns[Rank] = Column;
					}
				}
			}
			if (NameColumn != INDEX_NONE)
			{
				for (int32 LineIndex = 1; LineIndex < Lines.Num() && Rows.Num() < 200; ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(Lines[LineIndex], Fields);
					if (Fields.Num() <= NameColumn)
					{
						continue;
					}
					const FString Permission = FRokUiTextTable::Resolve(FCString::Atoi(*Fields[NameColumn]));
					if (Permission.IsEmpty())
					{
						continue;
					}
					TArray<FString> Allowed;
					for (int32 Rank = 0; Rank < 5; ++Rank)
					{
						if (RankColumns[Rank] == INDEX_NONE || Fields.Num() <= RankColumns[Rank])
						{
							continue;
						}
						if (FCString::Atoi(*Fields[RankColumns[Rank]]) != 0)
						{
							Allowed.Add(FString::Printf(TEXT("R%d"), Rank + 1));
						}
					}
					FRokDefinitionRow Row;
					Row.Title = Permission;
					Row.Detail = Allowed.Num() == 0
						? FString(TEXT("允许: 无"))
						: FString::Printf(TEXT("允许: %s"), *FString::Join(Allowed, TEXT(" ")));
					Row.Tag = TEXT("权限");
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "GuildPurviewUnavailable",
			"联盟权限配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("GuildPurviewUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("GuildPurviewUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "GuildPurviewHeader", "联盟权限 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 项 · R1–R5 为联盟职位层级的权限列"), Rows.Num())),
		TEXT("GuildPurview"));
}

void URokUnityModalWidget::OpenItemCollection()
{
	// UI_Win_ItemCollection: item compendium. Local definition preview.
	ActiveMode = ERokUnityModalMode::ItemCollection;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenVipStore()
{
	// UI_Win_VipStore: VIP level shop. Local definition preview.
	ActiveMode = ERokUnityModalMode::VipStore;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenTavernBoxDesc()
{
	// UI_Win_TavernBoxDesc: tavern box probability disclosure. Preview only.
	ActiveMode = ERokUnityModalMode::TavernBoxDesc;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenTheWall()
{
	// UI_Win_TheWall: city wall durability by level. Local definition preview.
	ActiveMode = ERokUnityModalMode::TheWall;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenTheTower()
{
	// UI_Win_TheTower: guard tower stats and its HP-section buffs. Preview only.
	ActiveMode = ERokUnityModalMode::TheTower;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenGuildStore()
{
	// UI_Win_GuildStore: alliance shop rows. Local definition preview.
	ActiveMode = ERokUnityModalMode::GuildStore;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenGiftLimitedTime()
{
	// UI_Win_GiftLimitedTime: limited-time packs. Local definition preview.
	ActiveMode = ERokUnityModalMode::GiftLimitedTime;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenChargePop()
{
	// UI_Win_ChargePop: paid package list. Local definition preview.
	ActiveMode = ERokUnityModalMode::ChargePop;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenExpeditionStore()
{
	// UI_Win_ExpeditionStore: expedition shop rows. Local definition preview.
	ActiveMode = ERokUnityModalMode::ExpeditionStore;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenTavernReward()
{
	// UI_Win_TavernReward: tavern reward pool. Local definition preview.
	ActiveMode = ERokUnityModalMode::TavernReward;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenFuncGuide()
{
	// UI_Win_FuncGuide: tutorial step list. Local definition preview.
	ActiveMode = ERokUnityModalMode::FuncGuide;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenGuildGift()
{
	// UI_Win_GuildGift: alliance gift levels. Local definition preview.
	ActiveMode = ERokUnityModalMode::GuildGift;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenItemExchange()
{
	// UI_Win_ItemExchange: pick-one reward boxes. Local definition preview.
	ActiveMode = ERokUnityModalMode::ItemExchange;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenUiGallery()
{
	// Developer overview: every ported modal in one screen. Each button calls the
	// same public Open* entry point the command line uses.
	ActiveMode = ERokUnityModalMode::UiGallery;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::OpenVipPrivilege()
{
	// UI_Win_Vip: VIP privileges by level. Local definition preview.
	ActiveMode = ERokUnityModalMode::VipPrivilege;
	SelectedBuildingActor.Reset();
	RebuildModeContent(true);
	SetVisibility(ESlateVisibility::Visible);
}

void URokUnityModalWidget::BuildVipPrivilege()
{
	// UI_Win_Vip: VipAtt.csv lists one row per (levelGroup, attribute); `dec` is
	// the localized attribute name and `add` the percentage/value it grants.
	TArray<FRokDefinitionRow> Rows;
	int32 Total = 0;
	FString Csv;
	TArray<TArray<FString>> Records;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/VipAtt.csv"))) && !Csv.IsEmpty())
	{
		FRokCsv::ParseRecords(Csv, Records);
		if (Records.Num() > 1)
		{
			const TArray<FString>& Header = Records[0];
			const int32 LevelColumn = FRokCsv::FindColumn(Header, TEXT("levelGroup"));
			const int32 DescColumn = FRokCsv::FindColumn(Header, TEXT("dec"));
			const int32 AddColumn = FRokCsv::FindColumn(Header, TEXT("add"));
			if (LevelColumn != INDEX_NONE && DescColumn != INDEX_NONE && AddColumn != INDEX_NONE)
			{
				const int32 Required = FMath::Max(LevelColumn, FMath::Max(DescColumn, AddColumn));
				for (int32 Index = 1; Index < Records.Num(); ++Index)
				{
					const TArray<FString>& Fields = Records[Index];
					if (Fields.Num() <= Required)
					{
						continue;
					}
					const FString Attribute = FRokUiTextTable::SanitizeRichText(Fields[DescColumn]);
					const FString Value = Fields[AddColumn].TrimStartAndEnd();
					if (Attribute.IsEmpty() || Value.IsEmpty() || FCString::Atod(*Value) <= 0.0)
					{
						continue;
					}
					++Total;
					if (Rows.Num() >= 200)
					{
						continue;
					}
					FRokDefinitionRow Row;
					Row.Title = Attribute;
					Row.Detail = FString::Printf(TEXT("+%s"), *Value);
					Row.Tag = FString::Printf(TEXT("VIP %s"), *Fields[LevelColumn].TrimStartAndEnd());
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "VipPrivilegeUnavailable",
			"贵宾特权配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("VipPrivilegeUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("VipPrivilegeUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "VipPrivilegeHeader", "贵宾特权 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 条 · 实际 VIP 等级与经验由服务器决定"), Total)),
		TEXT("VipPrivilege"),
		700.0f,
		76.0f,
		false,
		true);
}

void URokUnityModalWidget::BuildUiGallery()
{
	// Developer gallery: the 19 entry buttons (URokGalleryButton, EntryIndex set
	// in the page) open the same windows as their -RokOpenModal= switches.
	URokUiPageWidget* Page = MountModePage(TEXT("UiGallery"));
	if (!Page)
	{
		return;
	}
	constexpr int32 EntryCount = 19;
	for (int32 Index = 0; Index < EntryCount; ++Index)
	{
		if (URokGalleryButton* Button = Page->Get<URokGalleryButton>(IndexedName(TEXT("GalleryButton"), Index)))
		{
			Button->OnClicked.AddDynamic(Button, &URokGalleryButton::HandleClicked);
			Button->OnGalleryEntryClicked.AddDynamic(this, &URokUnityModalWidget::HandleUiGalleryEntry);
		}
	}
}

void URokUnityModalWidget::HandleUiGalleryEntry(int32 EntryIndex)
{
	switch (EntryIndex)
	{
	case 0: OpenGainEffect(0); break;
	case 1: OpenBattlePassRewards(); break;
	case 2: OpenEventDate(); break;
	case 3: OpenGuildPurview(); break;
	case 4: OpenChapterDialog(0); break;
	case 5: OpenGuildFortress(); break;
	case 6: OpenItemCollection(); break;
	case 7: OpenVipStore(); break;
	case 8: OpenTavernBoxDesc(); break;
	case 9: OpenTheWall(); break;
	case 10: OpenTheTower(); break;
	case 11: OpenGuildStore(); break;
	case 12: OpenGiftLimitedTime(); break;
	case 13: OpenChargePop(); break;
	case 14: OpenExpeditionStore(); break;
	case 15: OpenTavernReward(); break;
	case 16: OpenFuncGuide(); break;
	case 17: OpenGuildGift(); break;
	case 18: OpenItemExchange(); break;
	default: break;
	}
}

void URokUnityModalWidget::BuildItemExchange()
{
	// UI_Win_ItemExchange: the prefab is a quantity converter (`btn_add`,
	// `btn_substract`, `ipt_num`, `lbl_changeNum`). Rows come from
	// ItemRewardChoice.csv; the stepper edits a local count for the selected row
	// and does not invent exchange rates.
	TArray<FRokDefinitionRow> Rows;
	ActiveExchangeEntries.Reset();
	FString Csv;
	TArray<TArray<FString>> Records;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/ItemRewardChoice.csv"))) && !Csv.IsEmpty())
	{
		FRokCsv::ParseRecords(Csv, Records);
		if (Records.Num() > 1)
		{
			const TArray<FString>& Header = Records[0];
			const int32 GroupColumn = FRokCsv::FindColumn(Header, TEXT("group"));
			const int32 ItemColumn = FRokCsv::FindColumn(Header, TEXT("item"));
			const int32 NumColumn = FRokCsv::FindColumn(Header, TEXT("num"));
			if (ItemColumn != INDEX_NONE)
			{
				for (int32 Index = 1; Index < Records.Num() && Rows.Num() < 200; ++Index)
				{
					const TArray<FString>& Fields = Records[Index];
					if (Fields.Num() <= ItemColumn)
					{
						continue;
					}
					const FString EntryName = FRokUiTextTable::SanitizeRichText(Fields[ItemColumn]);
					if (EntryName.IsEmpty())
					{
						continue;
					}
					const int32 Count = NumColumn != INDEX_NONE && Fields.Num() > NumColumn
						? FCString::Atoi(*Fields[NumColumn]) : 0;
					FRokDefinitionRow Row;
					Row.Title = EntryName;
					Row.Detail = GroupColumn != INDEX_NONE && Fields.Num() > GroupColumn
						? FString::Printf(TEXT("组 %s · 数量 %d"), *Fields[GroupColumn].TrimStartAndEnd(),
							FMath::Max(1, Count))
						: FString::Printf(TEXT("数量 %d"), FMath::Max(1, Count));
					ActiveExchangeEntries.Add(Row.Title);
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "ItemExchangeUnavailable",
			"自选宝箱配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("ItemExchangeUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("ItemExchangeUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}

	// The scroll, stepper and footer live in the page WBP; each entry is a row WBP.
	URokUiPageWidget* Page = MountModePage(TEXT("ItemExchange"));
	if (!Page)
	{
		return;
	}
	if (USizeBox* Size = Page->Get<USizeBox>(TEXT("ItemExchangeScrollSize")))
	{
		Size->SetHeightOverride(Rows.Num() * 60.0f);
	}
	UCanvasPanel* Canvas = Page->Get<UCanvasPanel>(TEXT("ItemExchangeScrollContent"));
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FString Suffix = FString::Printf(TEXT("_%d"), Index);
		URokUiPageWidget* Row = MountItem(TEXT("ItemExchangeRow"), Canvas, *(TEXT("ItemExchangeRow") + Suffix),
			FVector2D(0.0f, Index * 60.0f), FVector2D(440.0f, 52.0f), 3, TEXT("ItemExchange"), Suffix);
		if (!Row)
		{
			break;
		}
		SetPageText(Row, TEXT("RowTitle"), FText::FromString(Rows[Index].Title));
		SetPageText(Row, TEXT("RowDetail"), FText::FromString(Rows[Index].Detail));
	}

	// Right: selection + quantity stepper (`btn_substract` / `ipt_num` / `btn_add`).
	if (ItemExchangeIndex < 0 || ItemExchangeIndex >= ActiveExchangeEntries.Num())
	{
		ItemExchangeIndex = 0;
	}
	ItemExchangeCount = FMath::Clamp(ItemExchangeCount, 1, 99);
	if (UButton* Minus = Page->Get<UButton>(TEXT("ItemExchangeMinus")))
	{
		Minus->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleExchangeMinus);
	}
	if (UButton* Plus = Page->Get<UButton>(TEXT("ItemExchangePlus")))
	{
		Plus->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleExchangePlus);
	}
	SetPageText(Page, TEXT("ItemExchangeFooter"), FText::FromString(FString::Printf(
		TEXT("共 %d 项 · 数量为本地预览，兑换消耗与到账由服务器决定"), ActiveExchangeEntries.Num())));
	RefreshExchangeSummary();
}

void URokUnityModalWidget::RefreshExchangeSummary()
{
	const FString EntryName = ActiveExchangeEntries.IsValidIndex(ItemExchangeIndex)
		? ActiveExchangeEntries[ItemExchangeIndex] : FString(TEXT("-"));
	if (UTextBlock* Name = Cast<UTextBlock>(RokUiFind::Find(this, TEXT("ItemExchangeSelectedName"))))
	{
		Name->SetText(FText::FromString(FString::Printf(TEXT("已选：%s"), *EntryName)));
	}
	if (UTextBlock* Count = Cast<UTextBlock>(RokUiFind::Find(this, TEXT("ItemExchangeCount"))))
	{
		Count->SetText(FText::FromString(FString::Printf(TEXT("%d"), ItemExchangeCount)));
	}
	if (UTextBlock* Change = Cast<UTextBlock>(RokUiFind::Find(this, TEXT("ItemExchangeChange"))))
	{
		Change->SetText(FText::FromString(FString::Printf(
			TEXT("数量 %d × %s\n兑换比例与消耗为服务器配置，本地不推算。"),
			ItemExchangeCount, *EntryName)));
	}
}

void URokUnityModalWidget::HandleExchangePlus()
{
	ItemExchangeCount = FMath::Min(ItemExchangeCount + 1, 99);
	RefreshExchangeSummary();
}

void URokUnityModalWidget::HandleExchangeMinus()
{
	ItemExchangeCount = FMath::Max(ItemExchangeCount - 1, 1);
	RefreshExchangeSummary();
}


void URokUnityModalWidget::BuildGuildGift()
{
	// UI_Win_GuildGift: AllianceGiftReward.csv (giftType/giftLevel/reward/
	// giftPoint/keyPoint) joined with AllianceGiftType.csv (`l_nameId` -> 宝箱名).
	TMap<int32, FString> TypeNames;
	FString TypeCsv;
	TArray<TArray<FString>> TypeRecords;
	if (FFileHelper::LoadFileToString(TypeCsv, *RokDataPaths::Resolve(TEXT("ConfigTables/AllianceGiftType.csv"))) && !TypeCsv.IsEmpty())
	{
		FRokCsv::ParseRecords(TypeCsv, TypeRecords);
		if (TypeRecords.Num() > 1)
		{
			const int32 IdColumn = FRokCsv::FindColumn(TypeRecords[0], TEXT("ID"));
			const int32 NameColumn = FRokCsv::FindColumn(TypeRecords[0], TEXT("l_nameId"));
			if (IdColumn != INDEX_NONE && NameColumn != INDEX_NONE)
			{
				for (int32 Index = 1; Index < TypeRecords.Num(); ++Index)
				{
					const TArray<FString>& Fields = TypeRecords[Index];
					if (Fields.Num() <= FMath::Max(IdColumn, NameColumn))
					{
						continue;
					}
					const FString Name = FRokUiTextTable::Resolve(FCString::Atoi(*Fields[NameColumn]));
					if (!Name.IsEmpty())
					{
						TypeNames.Add(FCString::Atoi(*Fields[IdColumn]), Name);
					}
				}
			}
		}
	}

	TArray<FRokDefinitionRow> Rows;
	FString Csv;
	TArray<TArray<FString>> Records;
	int32 Total = 0;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/AllianceGiftReward.csv"))) && !Csv.IsEmpty())
	{
		FRokCsv::ParseRecords(Csv, Records);
		if (Records.Num() > 1)
		{
			const TArray<FString>& Header = Records[0];
			const int32 TypeColumn = FRokCsv::FindColumn(Header, TEXT("giftType"));
			const int32 LevelColumn = FRokCsv::FindColumn(Header, TEXT("giftLevel"));
			const int32 RewardColumn = FRokCsv::FindColumn(Header, TEXT("reward"));
			const int32 PointColumn = FRokCsv::FindColumn(Header, TEXT("giftPoint"));
			const int32 KeyColumn = FRokCsv::FindColumn(Header, TEXT("keyPoint"));
			if (TypeColumn != INDEX_NONE && LevelColumn != INDEX_NONE)
			{
				const int32 Required = FMath::Max(TypeColumn, LevelColumn);
				for (int32 Index = 1; Index < Records.Num(); ++Index)
				{
					const TArray<FString>& Fields = Records[Index];
					if (Fields.Num() <= Required)
					{
						continue;
					}
					const int32 GiftType = FCString::Atoi(*Fields[TypeColumn]);
					const FString* TypeName = TypeNames.Find(GiftType);
					if (!TypeName)
					{
						continue;
					}
					const int32 Level = FCString::Atoi(*Fields[LevelColumn]);
					if (Level <= 0)
					{
						continue;
					}
					++Total;
					if (Rows.Num() >= 200)
					{
						continue;
					}
					FRokDefinitionRow Row;
					Row.Title = FString::Printf(TEXT("%s Lv%d"), **TypeName, Level);
					Row.Detail = FString::Printf(TEXT("礼物积分 %s · 钥匙积分 %s"),
						PointColumn != INDEX_NONE && Fields.Num() > PointColumn
							? *Fields[PointColumn].TrimStartAndEnd() : TEXT("-"),
						KeyColumn != INDEX_NONE && Fields.Num() > KeyColumn
							? *Fields[KeyColumn].TrimStartAndEnd() : TEXT("-"));
					Row.Tag = RewardColumn != INDEX_NONE && Fields.Num() > RewardColumn
						? FString::Printf(TEXT("奖励 %s"), *Fields[RewardColumn].TrimStartAndEnd())
						: TEXT("奖励");
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "GuildGiftUnavailable",
			"联盟礼物配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("GuildGiftUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("GuildGiftUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "GuildGiftHeader", "联盟礼物 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 档 · 领取需要服务器联盟礼物状态"), Total)),
		TEXT("GuildGift"));
}

void URokUnityModalWidget::BuildFuncGuide()
{
	// UI_Win_FuncGuide: Guide.csv rows. The table repeats the `step` header; the
	// first occurrence holds the Chinese step title. Read through the record
	// reader so quoted commas stay intact.
	TArray<FRokDefinitionRow> Rows;
	FString Csv;
	TArray<TArray<FString>> Records;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/Guide.csv"))) && !Csv.IsEmpty())
	{
		FRokCsv::ParseRecords(Csv, Records);
		if (Records.Num() > 1)
		{
			const TArray<FString>& Header = Records[0];
			const int32 StageColumn = FRokCsv::FindColumn(Header, TEXT("stage"));
			const int32 StepColumn = FRokCsv::FindColumn(Header, TEXT("step"));
			const int32 UiColumn = FRokCsv::FindColumn(Header, TEXT("uiId"));
			if (StepColumn != INDEX_NONE)
			{
				for (int32 Index = 1; Index < Records.Num() && Rows.Num() < 200; ++Index)
				{
					const TArray<FString>& Fields = Records[Index];
					if (Fields.Num() <= StepColumn)
					{
						continue;
					}
					const FString StepName = FRokUiTextTable::SanitizeRichText(Fields[StepColumn]);
					if (StepName.IsEmpty())
					{
						continue;
					}
					FRokDefinitionRow Row;
					Row.Title = StepName;
					const int32 Stage = StageColumn != INDEX_NONE && Fields.Num() > StageColumn
						? FCString::Atoi(*Fields[StageColumn]) : 0;
					const int32 UiId = UiColumn != INDEX_NONE && Fields.Num() > UiColumn
						? FCString::Atoi(*Fields[UiColumn]) : 0;
					Row.Detail = UiId > 0
						? FString::Printf(TEXT("阶段 %d · UI %d"), Stage, UiId)
						: FString::Printf(TEXT("阶段 %d"), Stage);
					Row.Tag = TEXT("引导");
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "FuncGuideUnavailable",
			"引导配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("FuncGuideUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("FuncGuideUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "FuncGuideHeader", "功能引导 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 步 · 实际引导由客户端流程驱动"), Rows.Num())),
		TEXT("FuncGuide"));
}

void URokUnityModalWidget::BuildTavernReward()
{
	// UI_Win_TavernReward: TavernRank.csv rows. The table repeats the `type`
	// header; entries carry their Chinese name in `comments`, the reward class in
	// `type`, and the chest in `remark`.
	// Commander entries (`typeData` holds the hero id) use `Hero.csv.heroIcon`.
	TMap<FString, FString> HeroIcons;
	{
		FString HeroCsv;
		TArray<TArray<FString>> HeroRecords;
		if (FFileHelper::LoadFileToString(HeroCsv, *RokDataPaths::Resolve(TEXT("ConfigTables/Hero.csv"))) && !HeroCsv.IsEmpty())
		{
			FRokCsv::ParseRecords(HeroCsv, HeroRecords);
			if (HeroRecords.Num() > 1)
			{
				const int32 IdColumn = FRokCsv::FindColumn(HeroRecords[0], TEXT("ID"));
				const int32 IconColumn = FRokCsv::FindColumn(HeroRecords[0], TEXT("heroIcon"));
				if (IdColumn != INDEX_NONE && IconColumn != INDEX_NONE)
				{
					for (int32 Index = 1; Index < HeroRecords.Num(); ++Index)
					{
						const TArray<FString>& Fields = HeroRecords[Index];
						if (Fields.Num() <= FMath::Max(IdColumn, IconColumn))
						{
							continue;
						}
						const FString Icon = Fields[IconColumn].TrimStartAndEnd();
						if (!Icon.IsEmpty())
						{
							HeroIcons.Add(Fields[IdColumn].TrimStartAndEnd(), Icon);
						}
					}
				}
			}
		}
	}
	// Item-type pool entries (`typeData` holds the item id) can show their icon.
	TMap<FString, FString> ItemIcons;
	{
		FString ItemCsv;
		TArray<TArray<FString>> ItemRecords;
		if (FFileHelper::LoadFileToString(ItemCsv, *RokDataPaths::Resolve(TEXT("ConfigTables/Item.csv"))) && !ItemCsv.IsEmpty())
		{
			FRokCsv::ParseRecords(ItemCsv, ItemRecords);
			if (ItemRecords.Num() > 1)
			{
				const int32 IdColumn = FRokCsv::FindColumn(ItemRecords[0], TEXT("ID"));
				const int32 IconColumn = FRokCsv::FindColumn(ItemRecords[0], TEXT("itemIcon"));
				if (IdColumn != INDEX_NONE && IconColumn != INDEX_NONE)
				{
					for (int32 Index = 1; Index < ItemRecords.Num(); ++Index)
					{
						const TArray<FString>& Fields = ItemRecords[Index];
						if (Fields.Num() <= FMath::Max(IdColumn, IconColumn))
						{
							continue;
						}
						const FString Icon = Fields[IconColumn].TrimStartAndEnd();
						if (!Icon.IsEmpty())
						{
							ItemIcons.Add(Fields[IdColumn].TrimStartAndEnd(), Icon);
						}
					}
				}
			}
		}
	}
	TArray<FRokDefinitionRow> Rows;
	FString Csv;
	TArray<TArray<FString>> Records;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/TavernRank.csv"))) && !Csv.IsEmpty())
	{
		FRokCsv::ParseRecords(Csv, Records);
		if (Records.Num() > 1)
		{
			const TArray<FString>& Header = Records[0];
			const int32 GroupColumn = FRokCsv::FindColumn(Header, TEXT("group"));
			const int32 TypeColumn = FRokCsv::FindColumn(Header, TEXT("type"));
			const int32 CommentColumn = FRokCsv::FindColumn(Header, TEXT("comments"));
			const int32 RemarkColumn = FRokCsv::FindColumn(Header, TEXT("remark"));
			const int32 TypeDataColumn = FRokCsv::FindColumn(Header, TEXT("typeData"));
			if (TypeColumn != INDEX_NONE && CommentColumn != INDEX_NONE)
			{
				const int32 Required = FMath::Max(TypeColumn, CommentColumn);
				for (int32 Index = 1; Index < Records.Num() && Rows.Num() < 200; ++Index)
				{
					const TArray<FString>& Fields = Records[Index];
					if (Fields.Num() <= Required)
					{
						continue;
					}
					const FString EntryName = FRokUiTextTable::SanitizeRichText(Fields[CommentColumn]);
					if (EntryName.IsEmpty())
					{
						continue;
					}
					FRokDefinitionRow Row;
					Row.Title = EntryName;
					Row.Detail = FString::Printf(TEXT("%s · %s"),
						*FRokUiTextTable::SanitizeRichText(Fields[TypeColumn]),
						GroupColumn != INDEX_NONE && Fields.Num() > GroupColumn
							? *FString::Printf(TEXT("池 %s"), *Fields[GroupColumn].TrimStartAndEnd())
							: TEXT("卡池"));
					if (RemarkColumn != INDEX_NONE && Fields.Num() > RemarkColumn)
					{
						Row.Tag = FRokUiTextTable::SanitizeRichText(Fields[RemarkColumn]);
					}
					if (TypeDataColumn != INDEX_NONE && Fields.Num() > TypeDataColumn)
					{
						const FString TypeData = Fields[TypeDataColumn].TrimStartAndEnd();
						if (const FString* Icon = ItemIcons.Find(TypeData))
						{
							Row.Icon = *Icon;
						}
						else if (const FString* HeroIcon = HeroIcons.Find(TypeData))
						{
							Row.Icon = *HeroIcon;
						}
					}
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "TavernRewardUnavailable",
			"酒馆奖励池配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("TavernRewardUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("TavernRewardUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "TavernRewardHeader", "酒馆奖励池 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 项 · 实际抽取结果以服务器为准"), Rows.Num())),
		TEXT("TavernReward"));
}

void URokUnityModalWidget::BuildExpeditionStore()
{
	// UI_Win_ExpeditionStore: ExpeditionShop.csv (itemID/number/price/groupID)
	// joined with Item.csv -> Language.csv. The table repeats one item per group,
	// so rows keep the first occurrence per (group, item, price) tuple.
	TMap<FString, FString> ItemNames;
	TMap<FString, FString> ItemIcons;
	FString ItemCsv;
	TArray<TArray<FString>> ItemRecords;
	if (FFileHelper::LoadFileToString(ItemCsv, *RokDataPaths::Resolve(TEXT("ConfigTables/Item.csv"))) && !ItemCsv.IsEmpty())
	{
		FRokCsv::ParseRecords(ItemCsv, ItemRecords);
		if (ItemRecords.Num() > 1)
		{
			const int32 IdColumn = FRokCsv::FindColumn(ItemRecords[0], TEXT("ID"));
			const int32 NameColumn = FRokCsv::FindColumn(ItemRecords[0], TEXT("l_nameID"));
			const int32 IconColumn = FRokCsv::FindColumn(ItemRecords[0], TEXT("itemIcon"));
			if (IdColumn != INDEX_NONE && NameColumn != INDEX_NONE)
			{
				for (int32 Index = 1; Index < ItemRecords.Num(); ++Index)
				{
					const TArray<FString>& Fields = ItemRecords[Index];
					if (Fields.Num() <= FMath::Max(IdColumn, NameColumn))
					{
						continue;
					}
					const FString ItemName = FRokUiTextTable::Resolve(FCString::Atoi(*Fields[NameColumn]));
					if (!ItemName.IsEmpty())
					{
						const FString ItemId = Fields[IdColumn].TrimStartAndEnd();
						ItemNames.Add(ItemId, ItemName);
						if (IconColumn != INDEX_NONE && Fields.Num() > IconColumn)
						{
							ItemIcons.Add(ItemId, Fields[IconColumn].TrimStartAndEnd());
						}
					}
				}
			}
		}
	}

	TArray<FRokDefinitionRow> Rows;
	TArray<FString> Lines;
	FString Csv;
	int32 Total = 0;
	TSet<FString> SeenKeys;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/ExpeditionShop.csv"))) && !Csv.IsEmpty())
	{
		Csv.ParseIntoArrayLines(Lines, false);
		if (Lines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(Lines[0], Columns);
			const int32 GroupColumn = FRokCsv::FindColumn(Columns, TEXT("groupID"));
			const int32 ItemColumn = FRokCsv::FindColumn(Columns, TEXT("itemID"));
			const int32 NumberColumn = FRokCsv::FindColumn(Columns, TEXT("number"));
			const int32 PriceColumn = FRokCsv::FindColumn(Columns, TEXT("price"));
			if (ItemColumn != INDEX_NONE && PriceColumn != INDEX_NONE)
			{
				const int32 Required = FMath::Max(ItemColumn, PriceColumn);
				for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(Lines[LineIndex], Fields);
					if (Fields.Num() <= Required)
					{
						continue;
					}
					const FString ItemId = Fields[ItemColumn].TrimStartAndEnd();
					const FString* ItemName = ItemNames.Find(ItemId);
					if (!ItemName)
					{
						continue;
					}
					const int32 Price = FCString::Atoi(*Fields[PriceColumn]);
					if (Price <= 0)
					{
						continue;
					}
					const FString Group = GroupColumn != INDEX_NONE && Fields.Num() > GroupColumn
						? Fields[GroupColumn].TrimStartAndEnd() : FString(TEXT("-"));
					const FString Key = FString::Printf(TEXT("%s|%s|%d"), *Group, *ItemId, Price);
					++Total;
					if (SeenKeys.Contains(Key) || Rows.Num() >= 200)
					{
						continue;
					}
					SeenKeys.Add(Key);
					const int32 Number = NumberColumn != INDEX_NONE && Fields.Num() > NumberColumn
						? FCString::Atoi(*Fields[NumberColumn]) : 0;
					FRokDefinitionRow Row;
					Row.Title = *ItemName;
					if (const FString* IconRef = ItemIcons.Find(Fields[ItemColumn].TrimStartAndEnd()))
					{
						Row.Icon = *IconRef;
					}
					if (const FString* IconRef = ItemIcons.Find(Fields[ItemColumn].TrimStartAndEnd()))
					{
						Row.Icon = *IconRef;
					}
					Row.Detail = Number > 0
						? FString::Printf(TEXT("价格 %d · 数量 %d"), Price, Number)
						: FString::Printf(TEXT("价格 %d"), Price);
					Row.Tag = FString::Printf(TEXT("组 %s"), *Group);
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "ExpeditionStoreUnavailable",
			"远征商店配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("ExpeditionStoreUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("ExpeditionStoreUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "ExpeditionStoreHeader", "远征商店 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 条（去重后显示）；兑换需要服务器远征状态"), Total)),
		TEXT("ExpeditionStore"),
		700.0f,
		76.0f,
		false,
		true);
}

void URokUnityModalWidget::BuildChargePop()
{
	// UI_Win_ChargePop: RechargeSale.csv rows (name/price/discount/label). The
	// table repeats `l_nameID` and has quoted multi-line descriptions, so it is
	// read through FRokCsv::ParseRecords with first-match column lookup.
	TArray<FRokDefinitionRow> Rows;
	FString Csv;
	TArray<TArray<FString>> Records;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/RechargeSale.csv"))) && !Csv.IsEmpty())
	{
		FRokCsv::ParseRecords(Csv, Records);
		if (Records.Num() > 1)
		{
			const TArray<FString>& Header = Records[0];
			const int32 NameColumn = FRokCsv::FindColumn(Header, TEXT("name"));
			const int32 PriceColumn = FRokCsv::FindColumn(Header, TEXT("price"));
			const int32 DiscountColumn = FRokCsv::FindColumn(Header, TEXT("discount"));
			const int32 LabelColumn = FRokCsv::FindColumn(Header, TEXT("label"));
			if (NameColumn != INDEX_NONE && PriceColumn != INDEX_NONE)
			{
				const int32 Required = FMath::Max(NameColumn, PriceColumn);
				for (int32 Index = 1; Index < Records.Num() && Rows.Num() < 200; ++Index)
				{
					const TArray<FString>& Fields = Records[Index];
					if (Fields.Num() <= Required)
					{
						continue;
					}
					const FString PackName = FRokUiTextTable::SanitizeRichText(Fields[NameColumn]);
					const int32 Price = FCString::Atoi(*Fields[PriceColumn]);
					if (PackName.IsEmpty() || Price <= 0)
					{
						continue;
					}
					FRokDefinitionRow Row;
					Row.Title = PackName;
					Row.Detail = FString::Printf(TEXT("价格 %d"), Price);
					FString Tag;
					if (DiscountColumn != INDEX_NONE && Fields.Num() > DiscountColumn)
					{
						Tag = FRokUiTextTable::SanitizeRichText(Fields[DiscountColumn]);
					}
					if (LabelColumn != INDEX_NONE && Fields.Num() > LabelColumn && !Fields[LabelColumn].TrimStartAndEnd().IsEmpty())
					{
						const FString Label = FRokUiTextTable::SanitizeRichText(Fields[LabelColumn]);
						Tag = Tag.IsEmpty() ? Label : FString::Printf(TEXT("%s · %s"), *Tag, *Label);
					}
					// Templated values like {0}% lose their placeholder; drop empty tags.
					if (!Tag.Contains(TEXT("0")) && !Tag.Contains(TEXT("1")) && Tag.Len() <= 2)
					{
						Tag.Reset();
					}
					Row.Tag = Tag;
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "ChargePopUnavailable",
			"充值礼包配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("ChargePopUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("ChargePopUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "ChargePopHeader", "充值礼包 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 个 · 购买需要服务器商店状态"), Rows.Num())),
		TEXT("ChargePop"));
}

void URokUnityModalWidget::BuildGiftLimitedTime()
{
	// UI_Win_GiftLimitedTime: RechargeLimitTimeBag.csv rows. The `des` column is a
	// multi-line quoted blurb, so this uses FRokCsv::ParseRecords instead of the
	// per-line splitter.
	TArray<FRokDefinitionRow> Rows;
	FString Csv;
	TArray<TArray<FString>> Records;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/RechargeLimitTimeBag.csv"))) && !Csv.IsEmpty())
	{
		FRokCsv::ParseRecords(Csv, Records);
		if (Records.Num() > 1)
		{
			const TArray<FString>& Header = Records[0];
			const int32 NameColumn = FRokCsv::FindColumn(Header, TEXT("name"));
			const int32 PriceColumn = FRokCsv::FindColumn(Header, TEXT("price"));
			const int32 LevelColumn = FRokCsv::FindColumn(Header, TEXT("limitLvMin"));
			const int32 TypeColumn = FRokCsv::FindColumn(Header, TEXT("type"));
			if (NameColumn != INDEX_NONE && PriceColumn != INDEX_NONE)
			{
				const int32 Required = FMath::Max(NameColumn, PriceColumn);
				for (int32 Index = 1; Index < Records.Num() && Rows.Num() < 200; ++Index)
				{
					const TArray<FString>& Fields = Records[Index];
					if (Fields.Num() <= Required)
					{
						continue;
					}
					const FString PackName = FRokUiTextTable::SanitizeRichText(Fields[NameColumn]);
					const int32 Price = FCString::Atoi(*Fields[PriceColumn]);
					if (PackName.IsEmpty() || Price <= 0)
					{
						continue;
					}
					FRokDefinitionRow Row;
					Row.Title = PackName;
					const int32 MinLevel = LevelColumn != INDEX_NONE && Fields.Num() > LevelColumn
						? FCString::Atoi(*Fields[LevelColumn]) : -1;
					Row.Detail = MinLevel > 0
						? FString::Printf(TEXT("价格 %d · 解锁等级 %d"), Price, MinLevel)
						: FString::Printf(TEXT("价格 %d · 解锁等级 不限"), Price);
					if (TypeColumn != INDEX_NONE && Fields.Num() > TypeColumn)
					{
						Row.Tag = FRokUiTextTable::SanitizeRichText(Fields[TypeColumn]);
					}
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "GiftLimitedTimeUnavailable",
			"限时礼包配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("GiftLimitedTimeUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("GiftLimitedTimeUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "GiftLimitedTimeHeader", "限时礼包 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 个 · 购买需要服务器商店状态"), Rows.Num())),
		TEXT("GiftLimitedTime"));
}

void URokUnityModalWidget::BuildGuildStore()
{
	// UI_Win_GuildStore: AllianceShopItem.csv (itemType/group/stockPrice/
	// sellingPrice) joined with Item.csv -> Language.csv for the item name.
	TMap<FString, FString> ItemNames;
	TMap<FString, FString> ItemIcons;
	FString ItemCsv;
	TArray<FString> ItemLines;
	if (FFileHelper::LoadFileToString(ItemCsv, *RokDataPaths::Resolve(TEXT("ConfigTables/Item.csv"))) && !ItemCsv.IsEmpty())
	{
		ItemCsv.ParseIntoArrayLines(ItemLines, false);
		if (ItemLines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(ItemLines[0], Columns);
			int32 IdColumn = INDEX_NONE;
			int32 NameColumn = INDEX_NONE;
			int32 IconColumn = INDEX_NONE;
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("ID")) IdColumn = Column;
				else if (Name == TEXT("l_nameId")) NameColumn = Column;
				else if (Name == TEXT("itemIcon")) IconColumn = Column;
			}
			if (IdColumn != INDEX_NONE && NameColumn != INDEX_NONE)
			{
				for (int32 LineIndex = 1; LineIndex < ItemLines.Num(); ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(ItemLines[LineIndex], Fields);
					if (Fields.Num() <= FMath::Max(IdColumn, NameColumn))
					{
						continue;
					}
					const FString ItemName = FRokUiTextTable::Resolve(FCString::Atoi(*Fields[NameColumn]));
					if (!ItemName.IsEmpty())
					{
					const FString ItemId = Fields[IdColumn].TrimStartAndEnd();
					ItemNames.Add(ItemId, ItemName);
					if (IconColumn != INDEX_NONE && Fields.Num() > IconColumn)
					{
						ItemIcons.Add(ItemId, Fields[IconColumn].TrimStartAndEnd());
					}
					}
				}
			}
		}
	}

	TArray<FRokDefinitionRow> Rows;
	FString Csv;
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/AllianceShopItem.csv"))) && !Csv.IsEmpty())
	{
		Csv.ParseIntoArrayLines(Lines, false);
		if (Lines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(Lines[0], Columns);
			int32 ItemColumn = INDEX_NONE;
			int32 GroupColumn = INDEX_NONE;
			int32 StockColumn = INDEX_NONE;
			int32 SellColumn = INDEX_NONE;
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("itemType")) ItemColumn = Column;
				else if (Name == TEXT("group")) GroupColumn = Column;
				else if (Name == TEXT("stockPrice")) StockColumn = Column;
				else if (Name == TEXT("sellingPrice")) SellColumn = Column;
			}
			if (ItemColumn != INDEX_NONE && StockColumn != INDEX_NONE && SellColumn != INDEX_NONE)
			{
				for (int32 LineIndex = 1; LineIndex < Lines.Num() && Rows.Num() < 200; ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(Lines[LineIndex], Fields);
					if (Fields.Num() <= FMath::Max(ItemColumn, FMath::Max(StockColumn, SellColumn)))
					{
						continue;
					}
					const FString* ItemName = ItemNames.Find(Fields[ItemColumn].TrimStartAndEnd());
					if (!ItemName)
					{
						continue;
					}
					const int32 Stock = FCString::Atoi(*Fields[StockColumn]);
					const int32 Sell = FCString::Atoi(*Fields[SellColumn]);
					if (Stock <= 0 || Sell <= 0)
					{
						continue;
					}
					FRokDefinitionRow Row;
					Row.Title = *ItemName;
					if (const FString* IconRef = ItemIcons.Find(Fields[ItemColumn].TrimStartAndEnd()))
					{
						Row.Icon = *IconRef;
					}
					Row.Detail = FString::Printf(TEXT("进货价 %d · 售价 %d"), Stock, Sell);
					Row.Tag = GroupColumn != INDEX_NONE && Fields.Num() > GroupColumn
						? FString::Printf(TEXT("组 %s"), *Fields[GroupColumn].TrimStartAndEnd())
						: TEXT("商品");
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "GuildStoreUnavailable",
			"联盟商店配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("GuildStoreUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("GuildStoreUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "GuildStoreHeader", "联盟商店 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 件 · 兑换需要联盟币与服务器商店状态"), Rows.Num())),
		TEXT("GuildStore"));
}

void URokUnityModalWidget::BuildTheTower()
{
	// UI_Win_TheTower: the prefab is a status window (`pb_bar`, `img_Fill`,
	// `lbl_state`, `lbl_tiptext`), so this renders a bar + state + tip text
	// instead of the earlier level table.
	int32 MaxLevel = 0;
	int32 MaxAttack = 0;
	int32 MaxHp = 0;
	TArray<FRokDefinitionRow> Sections;
	FString Csv;
	TArray<TArray<FString>> Records;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/BuildingGuardTower.csv"))) && !Csv.IsEmpty())
	{
		FRokCsv::ParseRecords(Csv, Records);
		if (Records.Num() > 1)
		{
			const TArray<FString>& Header = Records[0];
			const int32 LevelColumn = FRokCsv::FindColumn(Header, TEXT("level"));
			const int32 AttackColumn = FRokCsv::FindColumn(Header, TEXT("warningTowerAttack"));
			const int32 HpColumn = FRokCsv::FindColumn(Header, TEXT("warningTowerHpMax"));
			if (LevelColumn != INDEX_NONE && AttackColumn != INDEX_NONE && HpColumn != INDEX_NONE)
			{
				for (int32 Index = 1; Index < Records.Num(); ++Index)
				{
					const TArray<FString>& Fields = Records[Index];
					if (Fields.Num() <= FMath::Max(LevelColumn, FMath::Max(AttackColumn, HpColumn)))
					{
						continue;
					}
					const int32 Level = FCString::Atoi(*Fields[LevelColumn]);
					const int32 Attack = FCString::Atoi(*Fields[AttackColumn]);
					const int32 Hp = FCString::Atoi(*Fields[HpColumn]);
					if (Level > MaxLevel)
					{
						MaxLevel = Level;
						MaxAttack = Attack;
						MaxHp = Hp;
					}
				}
			}
		}
	}

	FString AttrCsv;
	TArray<TArray<FString>> AttrRecords;
	if (FFileHelper::LoadFileToString(AttrCsv, *RokDataPaths::Resolve(TEXT("ConfigTables/GuardTowerAttr.csv"))) && !AttrCsv.IsEmpty())
	{
		FRokCsv::ParseRecords(AttrCsv, AttrRecords);
		if (AttrRecords.Num() > 1)
		{
			const TArray<FString>& Header = AttrRecords[0];
			const int32 SectionColumn = FRokCsv::FindColumn(Header, TEXT("hpSection"));
			const int32 ProtectColumn = FRokCsv::FindColumn(Header, TEXT("protectPer"));
			const int32 AttackColumn = FRokCsv::FindColumn(Header, TEXT("attackPer"));
			if (SectionColumn != INDEX_NONE && ProtectColumn != INDEX_NONE && AttackColumn != INDEX_NONE)
			{
				for (int32 Index = 1; Index < AttrRecords.Num(); ++Index)
				{
					const TArray<FString>& Fields = AttrRecords[Index];
					if (Fields.Num() <= FMath::Max(SectionColumn, FMath::Max(ProtectColumn, AttackColumn)))
					{
						continue;
					}
					FRokDefinitionRow Row;
					FString Section = Fields[SectionColumn].TrimStartAndEnd();
					Section.ReplaceInline(TEXT("|"), TEXT("%–"));
					if (!Section.Contains(TEXT("%")))
					{
						Section += TEXT("%");
					}
					Row.Title = FString::Printf(TEXT("血量 %s"), *Section);
					Row.Detail = FString::Printf(TEXT("减伤 %s%% · 攻击 %s%%"),
						*Fields[ProtectColumn].TrimStartAndEnd(),
						*Fields[AttackColumn].TrimStartAndEnd());
					Sections.Add(MoveTemp(Row));
				}
			}
		}
	}

	// UI_Win_TheTower layout (bar plate, `pb_bar`, `lbl_state`, `lbl_tiptext`)
	// lives in the page WBP; the state and tip lines come from the tables.
	URokUiPageWidget* Page = MountModePage(TEXT("TheTower"));
	if (!Page)
	{
		return;
	}
	if (MaxLevel <= 0)
	{
		for (const TCHAR* Name : {TEXT("TheTowerBarPlate"), TEXT("TheTowerHpBar"), TEXT("TheTowerStateText"), TEXT("TheTowerTipText")})
		{
			Page->SetShown(Page->Get<UWidget>(Name), false);
		}
		if (UCanvasPanel* PageCanvas = Cast<UCanvasPanel>(Page->GetRootWidget()))
		{
			const FText StatusMessage = NSLOCTEXT("RokModal", "TheTowerUnavailable",
				"警戒塔配置未发布或不可读取");
			if (!TryMountStatusMessage(PageCanvas, TEXT("TheTowerUnavailable"), TEXT("Standard"), StatusMessage))
			{
				UTextBlock* Missing = MakeModalText(TEXT("TheTowerUnavailable"), 16,
					FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
				Missing->SetText(StatusMessage);
				PlaceOnCanvas(PageCanvas, Missing, FVector2D(212.0f, 300.0f), FVector2D(700.0f, 60.0f), 3);
			}
		}
		return;
	}
	// No live HP locally: the bar stays full, as designed in the page.
	SetPageText(Page, TEXT("TheTowerStateText"), FText::FromString(FString::Printf(
		TEXT("警戒塔 %d 级 · 攻击 %d · 生命 %d（本地配置，满耐久）"),
		MaxLevel, MaxAttack, MaxHp)));
	// `lbl_tiptext` / `img_tips`: the HP-section rules, one per line.
	FString TipText = TEXT("生命分段生效：\n");
	for (const FRokDefinitionRow& Row : Sections)
	{
		TipText += FString::Printf(TEXT("%s —— %s\n"), *Row.Title, *Row.Detail);
	}
	TipText += TEXT("驻军、实时耐久与修复由服务器决定。");
	SetPageText(Page, TEXT("TheTowerTipText"), FText::FromString(TipText));
}

void URokUnityModalWidget::BuildTheWall()
{
	// UI_Win_TheWall: only wall durability is backed by data, so this renders a
	// status window (bar + level line + durability curve). The prefab's hero and
	// skill plates have no config table yet.
	int32 MaxLevel = 0;
	int32 MaxDurability = 0;
	TArray<TPair<int32, int32>> Curve;
	FString Csv;
	TArray<TArray<FString>> Records;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/BuildingCityWall.csv"))) && !Csv.IsEmpty())
	{
		FRokCsv::ParseRecords(Csv, Records);
		if (Records.Num() > 1)
		{
			const TArray<FString>& Header = Records[0];
			const int32 LevelColumn = FRokCsv::FindColumn(Header, TEXT("level"));
			const int32 DurabilityColumn = FRokCsv::FindColumn(Header, TEXT("wallDurableMax"));
			if (LevelColumn != INDEX_NONE && DurabilityColumn != INDEX_NONE)
			{
				for (int32 Index = 1; Index < Records.Num(); ++Index)
				{
					const TArray<FString>& Fields = Records[Index];
					if (Fields.Num() <= FMath::Max(LevelColumn, DurabilityColumn))
					{
						continue;
					}
					const int32 Level = FCString::Atoi(*Fields[LevelColumn]);
					const int32 Durability = FCString::Atoi(*Fields[DurabilityColumn]);
					if (Level <= 0 || Durability <= 0)
					{
						continue;
					}
					Curve.Emplace(Level, Durability);
					if (Level > MaxLevel)
					{
						MaxLevel = Level;
						MaxDurability = Durability;
					}
				}
			}
		}
	}

	// UI_Win_TheWall layout (bar plate, durability bar, state and tip lines)
	// lives in the page WBP; the lines come from the durability table.
	URokUiPageWidget* Page = MountModePage(TEXT("TheWall"));
	if (!Page)
	{
		return;
	}
	if (MaxLevel <= 0)
	{
		for (const TCHAR* Name : {TEXT("TheWallBarPlate"), TEXT("TheWallDurabilityBar"), TEXT("TheWallStateText"), TEXT("TheWallTipText")})
		{
			Page->SetShown(Page->Get<UWidget>(Name), false);
		}
		if (UCanvasPanel* PageCanvas = Cast<UCanvasPanel>(Page->GetRootWidget()))
		{
			const FText StatusMessage = NSLOCTEXT("RokModal", "TheWallUnavailable",
				"城墙耐久配置未发布或不可读取");
			if (!TryMountStatusMessage(PageCanvas, TEXT("TheWallUnavailable"), TEXT("Standard"), StatusMessage))
			{
				UTextBlock* Missing = MakeModalText(TEXT("TheWallUnavailable"), 16,
					FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
				Missing->SetText(StatusMessage);
				PlaceOnCanvas(PageCanvas, Missing, FVector2D(212.0f, 300.0f), FVector2D(700.0f, 60.0f), 3);
			}
		}
		return;
	}
	SetPageText(Page, TEXT("TheWallStateText"), FText::FromString(FString::Printf(
		TEXT("城墙 %d 级 · 耐久上限 %d（本地配置，满耐久）"), MaxLevel, MaxDurability)));
	// Milestones from the same curve, plus the honest note about the gap.
	FString TipText = TEXT("耐久曲线：");
	for (const TPair<int32, int32>& Entry : Curve)
	{
		if (Entry.Key == 1 || Entry.Key == MaxLevel || Entry.Key % 5 == 0)
		{
			TipText += FString::Printf(TEXT("%d 级 %d · "), Entry.Key, Entry.Value);
		}
	}
	TipText += FString::Printf(TEXT("\n共 %d 级 · 实际耐久、修复与守将由服务器决定。\n"), MaxLevel);
	TipText += TEXT("守将、技能槽位（img_skill1bg1..5 / btn_hero_add2）尚无对应配置表，未绘制。");
	SetPageText(Page, TEXT("TheWallTipText"), FText::FromString(TipText));
}

void URokUnityModalWidget::BuildTavernBoxDesc()
{
	// UI_Win_TavernBoxDesc: `sv_list` rows come from TavernProbility.csv
	// (name/l_nameID + probability, grouped by tavern tier).
	TArray<FRokDefinitionRow> Rows;
	FString Csv;
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/TavernProbility.csv"))) && !Csv.IsEmpty())
	{
		Csv.ParseIntoArrayLines(Lines, false);
		if (Lines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(Lines[0], Columns);
			int32 GroupColumn = INDEX_NONE;
			int32 NameColumn = INDEX_NONE;
			int32 NameIdColumn = INDEX_NONE;
			int32 ProbabilityColumn = INDEX_NONE;
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("group")) GroupColumn = Column;
				else if (Name == TEXT("name")) NameColumn = Column;
				else if (Name == TEXT("l_nameID")) NameIdColumn = Column;
				else if (Name == TEXT("probability")) ProbabilityColumn = Column;
			}
			if (ProbabilityColumn != INDEX_NONE && (NameColumn != INDEX_NONE || NameIdColumn != INDEX_NONE))
			{
				for (int32 LineIndex = 1; LineIndex < Lines.Num() && Rows.Num() < 200; ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(Lines[LineIndex], Fields);
					if (Fields.Num() <= ProbabilityColumn)
					{
						continue;
					}
					FString EntryName;
					if (NameColumn != INDEX_NONE && Fields.Num() > NameColumn)
					{
						EntryName = Fields[NameColumn].TrimStartAndEnd();
					}
					if (EntryName.IsEmpty() && NameIdColumn != INDEX_NONE && Fields.Num() > NameIdColumn)
					{
						EntryName = FRokUiTextTable::Resolve(FCString::Atoi(*Fields[NameIdColumn]));
					}
					const double Probability = FCString::Atod(*Fields[ProbabilityColumn]);
					if (EntryName.IsEmpty() || Probability <= 0.0)
					{
						continue;
					}
					FRokDefinitionRow Row;
					Row.Title = EntryName;
					Row.Detail = FString::Printf(TEXT("概率 %.2f%%"), Probability * 100.0);
					if (GroupColumn != INDEX_NONE && Fields.Num() > GroupColumn)
					{
						Row.Tag = FString::Printf(TEXT("组 %s"), *Fields[GroupColumn].TrimStartAndEnd());
					}
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "TavernBoxDescUnavailable",
			"酒馆概率配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("TavernBoxDescUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("TavernBoxDescUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "TavernBoxDescHeader", "酒馆宝箱概率 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 项 · 实际抽取结果以服务器为准"), Rows.Num())),
		TEXT("TavernBoxDesc"));
}

void URokUnityModalWidget::BuildVipStore()
{
	// UI_Win_VipStore: VipStore.csv rows carry the localized `name` directly
	// (vipLevel, price, number, discount); `itemID` gives the shop icon.
	TMap<FString, FString> ItemIcons;
	FString ItemCsv;
	TArray<TArray<FString>> ItemRecords;
	if (FFileHelper::LoadFileToString(ItemCsv, *RokDataPaths::Resolve(TEXT("ConfigTables/Item.csv"))) && !ItemCsv.IsEmpty())
	{
		FRokCsv::ParseRecords(ItemCsv, ItemRecords);
		if (ItemRecords.Num() > 1)
		{
			const int32 IdColumn = FRokCsv::FindColumn(ItemRecords[0], TEXT("ID"));
			const int32 IconColumn = FRokCsv::FindColumn(ItemRecords[0], TEXT("itemIcon"));
			if (IdColumn != INDEX_NONE && IconColumn != INDEX_NONE)
			{
				for (int32 Index = 1; Index < ItemRecords.Num(); ++Index)
				{
					const TArray<FString>& Fields = ItemRecords[Index];
					if (Fields.Num() <= FMath::Max(IdColumn, IconColumn))
					{
						continue;
					}
					const FString Icon = Fields[IconColumn].TrimStartAndEnd();
					if (!Icon.IsEmpty())
					{
						ItemIcons.Add(Fields[IdColumn].TrimStartAndEnd(), Icon);
					}
				}
			}
		}
	}

	TArray<FRokDefinitionRow> Rows;
	FString Csv;
	TArray<FString> Lines;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/VipStore.csv"))) && !Csv.IsEmpty())
	{
		Csv.ParseIntoArrayLines(Lines, false);
		if (Lines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(Lines[0], Columns);
			int32 VipColumn = INDEX_NONE;
			int32 ItemColumn = INDEX_NONE;
			int32 NameColumn = INDEX_NONE;
			int32 PriceColumn = INDEX_NONE;
			int32 NumberColumn = INDEX_NONE;
			int32 DiscountColumn = INDEX_NONE;
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("vipLevel")) VipColumn = Column;
				else if (Name == TEXT("itemID")) ItemColumn = Column;
				else if (Name == TEXT("name")) NameColumn = Column;
				else if (Name == TEXT("price")) PriceColumn = Column;
				else if (Name == TEXT("number")) NumberColumn = Column;
				else if (Name == TEXT("discount")) DiscountColumn = Column;
			}
			if (NameColumn != INDEX_NONE && PriceColumn != INDEX_NONE)
			{
				for (int32 LineIndex = 1; LineIndex < Lines.Num() && Rows.Num() < 200; ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(Lines[LineIndex], Fields);
					if (Fields.Num() <= FMath::Max(NameColumn, PriceColumn))
					{
						continue;
					}
					const FString ItemName = Fields[NameColumn].TrimStartAndEnd();
					const int32 Price = FCString::Atoi(*Fields[PriceColumn]);
					if (ItemName.IsEmpty() || Price <= 0)
					{
						continue;
					}
					FRokDefinitionRow Row;
					Row.Title = ItemName;
					Row.Detail = FString::Printf(TEXT("VIP %s · 价格 %d · 数量 %s"),
						VipColumn != INDEX_NONE && Fields.Num() > VipColumn
							? *Fields[VipColumn].TrimStartAndEnd() : TEXT("-"),
						Price,
						NumberColumn != INDEX_NONE && Fields.Num() > NumberColumn
							? *Fields[NumberColumn].TrimStartAndEnd() : TEXT("-"));
					if (DiscountColumn != INDEX_NONE && Fields.Num() > DiscountColumn)
					{
						Row.Tag = FString::Printf(TEXT("折扣 %s"), *Fields[DiscountColumn].TrimStartAndEnd());
					}
					if (ItemColumn != INDEX_NONE && Fields.Num() > ItemColumn)
					{
						if (const FString* Icon = ItemIcons.Find(Fields[ItemColumn].TrimStartAndEnd()))
						{
							Row.Icon = *Icon;
						}
					}
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "VipStoreUnavailable",
			"贵宾商店配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("VipStoreUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("VipStoreUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "VipStoreHeader", "贵宾商店 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 件 · 购买需要服务器商店状态"), Rows.Num())),
		TEXT("VipStore"));
}


void URokUnityModalWidget::BuildItemTypeFilterChips()
{
	// Two chip rows over the definition grid, laid out in a page WBP: 类型 0..5
	// and 品质 0..5 (0 = 全部). The active pair composes into a "类型 N|品质 M" filter.
	URokUiPageWidget* Page = MountModePage(TEXT("ItemCollectionFilters"));
	if (!Page)
	{
		return;
	}
	for (int32 Row = 0; Row < 2; ++Row)
	{
		const FString ChipBase = FString::Printf(TEXT("ItemCollection%sChip"), Row == 0 ? TEXT("类型") : TEXT("品质"));
		for (int32 Index = 0; Index <= 5; ++Index)
		{
			URokGalleryButton* Chip = Page->Get<URokGalleryButton>(IndexedName(*ChipBase, Index));
			if (!Chip)
			{
				continue;
			}
			Chip->OnClicked.AddDynamic(Chip, &URokGalleryButton::HandleClicked);
			// AddDynamic stringifies the expression, so a conditional member pointer
			// never binds; branch instead.
			if (Row == 0)
			{
				Chip->OnGalleryEntryClicked.AddDynamic(this, &URokUnityModalWidget::HandleItemTypeFilterChip);
			}
			else
			{
				Chip->OnGalleryEntryClicked.AddDynamic(this, &URokUnityModalWidget::HandleItemQualityFilterChip);
			}
		}
	}
}

void URokUnityModalWidget::ApplyItemFilter()
{
	FString Filter;
	if (ItemCollectionType > 0)
	{
		Filter = FString::Printf(TEXT("类型 %d"), ItemCollectionType);
	}
	if (ItemCollectionQuality > 0)
	{
		Filter += Filter.IsEmpty()
			? FString::Printf(TEXT("品质 %d"), ItemCollectionQuality)
			: FString::Printf(TEXT("|品质 %d"), ItemCollectionQuality);
	}
	SetDefinitionFilter(Filter);
}

void URokUnityModalWidget::HandleItemTypeFilterChip(int32 ChipIndex)
{
	ItemCollectionType = FMath::Max(0, ChipIndex);
	ApplyItemFilter();
}

void URokUnityModalWidget::HandleItemQualityFilterChip(int32 ChipIndex)
{
	ItemCollectionQuality = FMath::Max(0, ChipIndex);
	ApplyItemFilter();
}

void URokUnityModalWidget::BuildItemCollection()
{
	// UI_Win_ItemCollection: one row per Item.csv entry whose `l_nameID` resolves,
	// with the row's `type`/`quality` shown as detail and `subType` as the tag.
	TArray<FRokDefinitionRow> Rows;
	FString Csv;
	TArray<FString> Lines;
	int32 Total = 0;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/Item.csv"))) && !Csv.IsEmpty())
	{
		Csv.ParseIntoArrayLines(Lines, false);
		if (Lines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(Lines[0], Columns);
			int32 NameColumn = INDEX_NONE;
			int32 TypeColumn = INDEX_NONE;
			int32 QualityColumn = INDEX_NONE;
			int32 SubTypeColumn = INDEX_NONE;
			int32 IconColumn = INDEX_NONE;
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("l_nameId")) NameColumn = Column;
				else if (Name == TEXT("type")) TypeColumn = Column;
				else if (Name == TEXT("itemIcon")) IconColumn = Column;
				else if (Name == TEXT("quality")) QualityColumn = Column;
				else if (Name == TEXT("subType")) SubTypeColumn = Column;
			}
			if (NameColumn != INDEX_NONE)
			{
				for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(Lines[LineIndex], Fields);
					if (Fields.Num() <= NameColumn)
					{
						continue;
					}
					const FString ItemName = FRokUiTextTable::Resolve(FCString::Atoi(*Fields[NameColumn]));
					if (ItemName.IsEmpty())
					{
						continue;
					}
					++Total;
					if (Rows.Num() >= 200)
					{
						continue;
					}
					FRokDefinitionRow Row;
					Row.Title = ItemName;
					if (IconColumn != INDEX_NONE && Fields.Num() > IconColumn)
					{
						Row.Icon = Fields[IconColumn].TrimStartAndEnd();
					}
					Row.Detail = FString::Printf(TEXT("类型 %s · 品质 %s"),
						TypeColumn != INDEX_NONE && Fields.Num() > TypeColumn
							? *Fields[TypeColumn].TrimStartAndEnd() : TEXT("-"),
						QualityColumn != INDEX_NONE && Fields.Num() > QualityColumn
							? *Fields[QualityColumn].TrimStartAndEnd() : TEXT("-"));
					if (SubTypeColumn != INDEX_NONE && Fields.Num() > SubTypeColumn)
					{
						Row.Tag = Fields[SubTypeColumn].TrimStartAndEnd();
					}
					Rows.Add(MoveTemp(Row));
				}
			}
		}
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "ItemCollectionUnavailable",
			"道具图鉴配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("ItemCollectionUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("ItemCollectionUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "ItemCollectionHeader", "道具图鉴 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 件（显示前 200）· 持有与使用需服务器背包"), Total)),
		TEXT("ItemCollection"),
		700.0f,
		116.0f,
		false,
		true,
		3);
	ItemCollectionTotal = Total;
	BuildItemTypeFilterChips();
	int32 InitialType = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("RokItemType="), InitialType) && InitialType > 0)
	{
		SetDefinitionFilter(FString::Printf(TEXT("类型 %d"), InitialType));
	}
}

void URokUnityModalWidget::BuildGuildFortress()
{
	// UI_Win_GuildFortress: alliance structures and their build cost. Names come
	// from AllianceBuildingType.csv (`l_nameId`), costs from
	// AllianceBuildingData.csv. The data repeats one row per order, so this keeps
	// the most expensive costed row per structure name.
	TMap<int32, FString> TypeNames;
	FString TypeCsv;
	TArray<FString> TypeLines;
	if (FFileHelper::LoadFileToString(TypeCsv, *RokDataPaths::Resolve(TEXT("ConfigTables/AllianceBuildingType.csv"))) && !TypeCsv.IsEmpty())
	{
		TypeCsv.ParseIntoArrayLines(TypeLines, false);
		if (TypeLines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(TypeLines[0], Columns);
			int32 TypeColumn = INDEX_NONE;
			int32 NameColumn = INDEX_NONE;
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("type")) TypeColumn = Column;
				else if (Name == TEXT("l_nameId")) NameColumn = Column;
			}
			if (TypeColumn != INDEX_NONE && NameColumn != INDEX_NONE)
			{
				for (int32 LineIndex = 1; LineIndex < TypeLines.Num(); ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(TypeLines[LineIndex], Fields);
					if (Fields.Num() <= FMath::Max(TypeColumn, NameColumn))
					{
						continue;
					}
					const FString Name = FRokUiTextTable::Resolve(FCString::Atoi(*Fields[NameColumn]));
					if (!Name.IsEmpty())
					{
						TypeNames.Add(FCString::Atoi(*Fields[TypeColumn]), Name);
					}
				}
			}
		}
	}

	struct FStructureCost
	{
		int32 Order = 0;
		int32 Food = 0;
		int32 Wood = 0;
		int32 Stone = 0;
		int32 Coin = 0;
		int32 Weight = 0;
	};
	TMap<FString, FStructureCost> BestCosts;
	TArray<FString> Lines;
	FString Csv;
	if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/AllianceBuildingData.csv"))) && !Csv.IsEmpty())
	{
		Csv.ParseIntoArrayLines(Lines, false);
		if (Lines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(Lines[0], Columns);
			int32 TypeColumn = INDEX_NONE;
			int32 OrderColumn = INDEX_NONE;
			int32 FoodColumn = INDEX_NONE;
			int32 WoodColumn = INDEX_NONE;
			int32 StoneColumn = INDEX_NONE;
			int32 CoinColumn = INDEX_NONE;
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("type")) TypeColumn = Column;
				else if (Name == TEXT("orderNum")) OrderColumn = Column;
				else if (Name == TEXT("food")) FoodColumn = Column;
				else if (Name == TEXT("wood")) WoodColumn = Column;
				else if (Name == TEXT("stone")) StoneColumn = Column;
				else if (Name == TEXT("coin")) CoinColumn = Column;
			}
			const int32 Required = FMath::Max(FMath::Max(TypeColumn, FoodColumn),
				FMath::Max(WoodColumn, StoneColumn));
			if (Required != INDEX_NONE)
			{
				for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(Lines[LineIndex], Fields);
					if (Fields.Num() <= Required)
					{
						continue;
					}
					const FString* TypeName = TypeNames.Find(FCString::Atoi(*Fields[TypeColumn]));
					if (!TypeName)
					{
						continue;
					}
					FStructureCost Cost;
					Cost.Order = OrderColumn != INDEX_NONE && Fields.Num() > OrderColumn
						? FCString::Atoi(*Fields[OrderColumn]) : 0;
					Cost.Food = FCString::Atoi(*Fields[FoodColumn]);
					Cost.Wood = FCString::Atoi(*Fields[WoodColumn]);
					Cost.Stone = FCString::Atoi(*Fields[StoneColumn]);
					Cost.Coin = CoinColumn != INDEX_NONE && Fields.Num() > CoinColumn
						? FCString::Atoi(*Fields[CoinColumn]) : 0;
					Cost.Weight = Cost.Food + Cost.Wood + Cost.Stone;
					const FStructureCost* Existing = BestCosts.Find(*TypeName);
					if (!Existing || Cost.Weight > Existing->Weight)
					{
						BestCosts.Add(*TypeName, Cost);
					}
				}
			}
		}
	}

	TArray<FRokDefinitionRow> Rows;
	BestCosts.KeySort([](const FString& A, const FString& B) { return A < B; });
	for (const TPair<FString, FStructureCost>& Pair : BestCosts)
	{
		if (Pair.Value.Weight <= 0)
		{
			continue;
		}
		FRokDefinitionRow Row;
		Row.Title = Pair.Value.Order > 0
			? FString::Printf(TEXT("%s · 最高序 %d"), *Pair.Key, Pair.Value.Order)
			: Pair.Key;
		Row.Detail = Pair.Value.Coin > 0
			? FString::Printf(TEXT("粮 %d · 木 %d · 石 %d · 币 %d"),
				Pair.Value.Food, Pair.Value.Wood, Pair.Value.Stone, Pair.Value.Coin)
			: FString::Printf(TEXT("粮 %d · 木 %d · 石 %d"),
				Pair.Value.Food, Pair.Value.Wood, Pair.Value.Stone);
		Row.Tag = TEXT("资源");
		Rows.Add(MoveTemp(Row));
	}

	if (Rows.Num() == 0)
	{
		const FText StatusMessage = NSLOCTEXT("RokModal", "GuildFortressUnavailable",
			"联盟建筑配置未发布或不可读取");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("GuildFortressUnavailable"), TEXT("Standard"), StatusMessage))
		{
			UTextBlock* Missing = MakeModalText(TEXT("GuildFortressUnavailable"), 16,
				FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 300.0f),
				FVector2D(700.0f, 60.0f), 3);
		}
		return;
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "GuildFortressHeader", "联盟建筑建造 · 本地配置预览"),
		Rows,
		FText::FromString(FString::Printf(TEXT("共 %d 类结构 · 建造需要联盟资金与领地"), Rows.Num())),
		TEXT("GuildFortress"));
}

void URokUnityModalWidget::BuildChapterDialog()
{
	// UI_Win_ChapterDialog: GroupId == 0 lists TaskChapter.csv chapters; a group id
	// renders that TaskDialog.csv group as a portrait dialogue screen.
	if (ChapterDialogGroup > 0)
	{
		TArray<FRokDefinitionRow> Rows;
		FString Csv;
		TArray<TArray<FString>> Records;
		if (FFileHelper::LoadFileToString(Csv, *RokDataPaths::Resolve(TEXT("ConfigTables/TaskDialog.csv")))
			&& !Csv.IsEmpty())
		{
			FRokCsv::ParseRecords(Csv, Records);
			if (Records.Num() > 1)
			{
				const TArray<FString>& Header = Records[0];
				const int32 GroupColumn = FRokCsv::FindColumn(Header, TEXT("group"));
				const int32 StepColumn = FRokCsv::FindColumn(Header, TEXT("step"));
				const int32 TextColumn = FRokCsv::FindColumn(Header, TEXT("text"));
				const int32 SideColumn = FRokCsv::FindColumn(Header, TEXT("leftOrRight"));
				const int32 ModelColumn = FRokCsv::FindColumn(Header, TEXT("npcAgeModel"));
				const int32 VoiceColumn = FRokCsv::FindColumn(Header, TEXT("soundClient"));
				if (GroupColumn != INDEX_NONE && TextColumn != INDEX_NONE)
				{
					// Collect first so the lines can be ordered by `step`.
					TArray<TPair<int32, FRokDialogLine>> Sorted;
					for (int32 Index = 1; Index < Records.Num(); ++Index)
					{
						const TArray<FString>& Fields = Records[Index];
						if (Fields.Num() <= FMath::Max(GroupColumn, TextColumn))
						{
							continue;
						}
						if (FCString::Atoi(*Fields[GroupColumn]) != ChapterDialogGroup)
						{
							continue;
						}
						FRokDialogLine Line;
						Line.Text = FRokUiTextTable::SanitizeRichText(
							FRokUiTextTable::Resolve(FCString::Atoi(*Fields[TextColumn])));
						if (Line.Text.IsEmpty())
						{
							Line.Text = FRokUiTextTable::SanitizeRichText(Fields[TextColumn]);
						}
						Line.bRight = SideColumn != INDEX_NONE && Fields.Num() > SideColumn
							&& FCString::Atoi(*Fields[SideColumn]) != 0;
						if (ModelColumn != INDEX_NONE && Fields.Num() > ModelColumn)
						{
							FString Model = Fields[ModelColumn];
							int32 PipeIndex = INDEX_NONE;
							if (Model.FindChar(TEXT('|'), PipeIndex))
							{
								Model.LeftInline(PipeIndex, EAllowShrinking::No);
							}
							Line.Model = Model.TrimStartAndEnd();
						}
						if (VoiceColumn != INDEX_NONE && Fields.Num() > VoiceColumn)
						{
							FString Voice = Fields[VoiceColumn];
							int32 PipeIndex = INDEX_NONE;
							if (Voice.FindChar(TEXT('|'), PipeIndex))
							{
								Voice.LeftInline(PipeIndex, EAllowShrinking::No);
							}
							Line.Voice = Voice.TrimStartAndEnd();
						}
						const int32 Step = StepColumn != INDEX_NONE && Fields.Num() > StepColumn
							? FCString::Atoi(*Fields[StepColumn]) : Index;
						Sorted.Emplace(Step, MoveTemp(Line));
					}
					Sorted.Sort([](const TPair<int32, FRokDialogLine>& A, const TPair<int32, FRokDialogLine>& B)
					{
						return A.Key < B.Key;
					});
					ActiveDialogLines.Reset();
					for (TPair<int32, FRokDialogLine>& Entry : Sorted)
					{
						ActiveDialogLines.Add(MoveTemp(Entry.Value));
					}
				}
			}
		}
		if (ActiveDialogLines.Num() > 0)
		{
			ActiveDialogIndex = 0;
			BuildChapterDialogScreen();
			return;
		}
		const FText MissingText = FText::FromString(FString::Printf(
			TEXT("对话组 %d 在 TaskDialog.csv 中没有可显示的台词"), ChapterDialogGroup));
		if (!URokUiSettings::IsLegacyForced())
		{
			if (URokUiPageWidget* EmptyPage = MountModePage(TEXT("ChapterDialogEmpty")))
			{
				if (UTextBlock* Missing = EmptyPage->Get<UTextBlock>(TEXT("ChapterDialogEmptyText")))
				{
					if (UCanvasPanelSlot* PageSlot = Cast<UCanvasPanelSlot>(EmptyPage->Slot))
					{
						// The error text must paint above the native frame while its transparent root leaves the close button usable.
						PageSlot->SetZOrder(5);
					}
					Missing->SetText(MissingText);
					return;
				}
				EmptyPage->RemoveFromParent();
			}
		}
		UTextBlock* Missing = MakeModalText(TEXT("ChapterDialogEmpty"), 16,
			FLinearColor(0.38f, 0.31f, 0.22f, 1.0f), false, ETextJustify::Center);
		Missing->SetText(MissingText);
		PlaceOnCanvas(ModeCanvas, Missing, FVector2D(212.0f, 400.0f),
			FVector2D(900.0f, 60.0f), 3);
		return;
	}

	// GroupId == 0: chapter list.
	TArray<FRokDefinitionRow> ChapterRows;
	FString ChapterCsv;
	TArray<FString> ChapterLines;
	if (FFileHelper::LoadFileToString(ChapterCsv, *RokDataPaths::Resolve(TEXT("ConfigTables/TaskChapter.csv")))
		&& !ChapterCsv.IsEmpty())
	{
		ChapterCsv.ParseIntoArrayLines(ChapterLines, false);
		if (ChapterLines.Num() > 1)
		{
			TArray<FString> Columns;
			SplitRokCsvLine(ChapterLines[0], Columns);
			int32 IdColumn = INDEX_NONE;
			int32 NameColumn = INDEX_NONE;
			int32 DescColumn = INDEX_NONE;
			for (int32 Column = 0; Column < Columns.Num(); ++Column)
			{
				const FString Name = Columns[Column].TrimStartAndEnd();
				if (Name == TEXT("ID")) IdColumn = Column;
				else if (Name == TEXT("l_nameId")) NameColumn = Column;
				else if (Name == TEXT("l_desc")) DescColumn = Column;
			}
			if (IdColumn != INDEX_NONE && NameColumn != INDEX_NONE)
			{
				for (int32 LineIndex = 1; LineIndex < ChapterLines.Num() && ChapterRows.Num() < 200; ++LineIndex)
				{
					TArray<FString> Fields;
					SplitRokCsvLine(ChapterLines[LineIndex], Fields);
					if (Fields.Num() <= FMath::Max(IdColumn, NameColumn))
					{
						continue;
					}
					const int32 Id = FCString::Atoi(*Fields[IdColumn]);
					const FString Name = FRokUiTextTable::SanitizeRichText(
						FRokUiTextTable::Resolve(FCString::Atoi(*Fields[NameColumn])));
					if (Id <= 0 || Name.IsEmpty())
					{
						continue;
					}
					FRokDefinitionRow Row;
					Row.Title = Name;
					Row.Detail = DescColumn != INDEX_NONE && Fields.Num() > DescColumn
						? FRokUiTextTable::SanitizeRichText(
							FRokUiTextTable::Resolve(FCString::Atoi(*Fields[DescColumn])))
						: FString();
					Row.Tag = FString::Printf(TEXT("章节 %d"), Id);
					ChapterRows.Add(MoveTemp(Row));
				}
			}
		}
	}
	BuildDefinitionList(
		NSLOCTEXT("RokModal", "ChapterDialogHeader", "剧情章节 · 本地配置预览"),
		ChapterRows,
		NSLOCTEXT("RokModal", "ChapterDialogFooter",
			"选择任意对话组可用 -RokChapterDialogGroup=<id> 直接打开台词"),
		TEXT("ChapterDialog"));
}

void URokUnityModalWidget::BuildChapterDialogScreen()
{
	// Portrait plates, the line text and the continue button live in the page
	// WBP; ShowDialogLine fills the line and speaker footer.
	URokUiPageWidget* Page = MountModePage(TEXT("ChapterDialogScreen"));
	if (!Page)
	{
		return;
	}
	if (UButton* Continue = Page->Get<UButton>(TEXT("ChapterDialogContinue")))
	{
		Continue->OnClicked.AddDynamic(this, &URokUnityModalWidget::AdvanceDialogLine);
	}
	ShowDialogLine();
}

void URokUnityModalWidget::ShowDialogLine()
{
	const FRokDialogLine* Line = ActiveDialogLines.IsValidIndex(ActiveDialogIndex)
		? &ActiveDialogLines[ActiveDialogIndex] : nullptr;
	if (!Line)
	{
		return;
	}
	if (UTextBlock* Text = Cast<UTextBlock>(RokUiFind::Find(this, TEXT("ChapterDialogLineText"))))
	{
		Text->SetText(FText::FromString(Line->Text));
	}
	if (UTextBlock* Speaker = Cast<UTextBlock>(RokUiFind::Find(this, TEXT("ChapterDialogSpeaker"))))
	{
		FString SpeakerLine = FString::Printf(TEXT("第 %d / %d 句"), ActiveDialogIndex + 1, ActiveDialogLines.Num());
		if (!Line->Model.IsEmpty())
		{
			// No portrait textures are imported yet; label the side that would show one.
			SpeakerLine += FString::Printf(TEXT(" · %s（%s）"), *Line->Model,
				Line->bRight ? TEXT("右侧立绘") : TEXT("左侧立绘"));
		}
		if (!Line->Voice.IsEmpty())
		{
			SpeakerLine += FString::Printf(TEXT(" · 语音 %s（未播放）"), *Line->Voice);
		}
		Speaker->SetText(FText::FromString(SpeakerLine));
	}
	// Only the speaking side's plate is emphasised.
	if (UCanvasPanel* LeftPlate = Cast<UCanvasPanel>(RokUiFind::Find(this, TEXT("ChapterPortraitLeftPlate"))))
	{
		LeftPlate->SetRenderOpacity(Line->bRight ? 0.35f : 1.0f);
	}
	if (UCanvasPanel* RightPlate = Cast<UCanvasPanel>(RokUiFind::Find(this, TEXT("ChapterPortraitRightPlate"))))
	{
		RightPlate->SetRenderOpacity(Line->bRight ? 1.0f : 0.35f);
	}
}

void URokUnityModalWidget::AdvanceDialogLine()
{
	if (ActiveDialogLines.Num() == 0)
	{
		return;
	}
	ActiveDialogIndex = FMath::Min(ActiveDialogIndex + 1, ActiveDialogLines.Num() - 1);
	ShowDialogLine();
}


void URokUnityModalWidget::AddBuildingPreviewGlow(UCanvasPanel* Canvas, TOptional<FVector2D> PreviewCenter)
{
	RokBuildingPreviewGlow::FData Data;
	FString Error;
	if (Data.Load(RokBuildingPreviewGlow::FData::DefaultPath(), Error))
	{
		auto* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/RokPrototype/UI/Materials/M_RokBuildingPreviewGlowUI.M_RokBuildingPreviewGlowUI"));
		auto* Glow = WidgetTree->ConstructWidget<URokBuildingPreviewGlowWidget>(URokBuildingPreviewGlowWidget::StaticClass(), TEXT("BuildingPreviewGlowWidget"));
		if (Glow->Initialize(Data, Material, Error))
		{
			// The extracted geometry is in the native upgrade window's coordinates.
			const FVector2D Origin = PreviewCenter.IsSet()
				? PreviewCenter.GetValue() - Data.GetLayers()[0].Center : FVector2D::ZeroVector;
			PlaceOnCanvas(Canvas, Glow, Origin, FVector2D(1440, 810), 1);
			Glow->SetPlaybackActive(true);
		}
	}
	if (!Error.IsEmpty()) UE_LOG(LogTemp, Error, TEXT("Building preview glow unavailable: %s"), *Error);
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
		State.UnityBuildingTypeId = 7;
		State.Level = 1;
		State.Power = 1;
	}

	const int32 ResourceType = ResourceBuildingReferenceType != 0 ? ResourceBuildingReferenceType : State.UnityBuildingTypeId;
	if (ResourceType >= 2 && ResourceType <= 5)
	{
		BuildResourceBuildingInfo(ResourceType, SelectedBuildingActor.IsValid() && Subsystem ? State.Level : 0);
		return;
	}

	// Layout, frames and stat labels live in the page WBP. The building preview
	// (art and glow chosen per building and age) is added into the page's canvas
	// at the place the C++ layout used.
	URokUiPageWidget* Page = MountModePage(TEXT("BuildingInfo"));
	if (!Page)
	{
		return;
	}
	if (UCanvasPanel* PageCanvas = Cast<UCanvasPanel>(Page->GetRootWidget()))
	{
		AddBuildingPreview(PageCanvas, TEXT("BuildingInfoImage"), State.UnityBuildingTypeId,
			FVector2D(208.8f, 166.9f), FVector2D(450.0f, 620.2f), FVector2D(438.8f, 507.0f), TEXT("BuildingInfoPreviewSlot"));
	}
	const int32 QueueCount = Subsystem ? Subsystem->GetOwnedBuildQueueCount() : 1;
	const float BuildSpeedBonus = Subsystem ? Subsystem->GetBuildSpeedBonusPercent() : 0.0f;
	SetPageText(Page, TEXT("BuildingInfoStatValue_0"), FText::AsNumber(QueueCount));
	SetPageText(Page, TEXT("BuildingInfoStatValue_1"), FText::AsNumber(State.Power));
	SetPageText(Page, TEXT("BuildingInfoStatValue_2"), FText::FromString(FString::Printf(TEXT("%.0f%%"), BuildSpeedBonus)));
	SetPageText(Page, TEXT("BuildingInfoDescription"), State.Type == ERokBuildingType::BuilderHut || !SelectedBuildingActor.IsValid()
		? NSLOCTEXT("RokModal", "BuilderHutDescription", "工人小屋是勤劳的建筑工人们日常生活和休息的地方，同时也是城市中唯一可以对建造队列进行管理的建筑物。在这里，您可以选择花费钻石雇佣更多的工人来增加一个额外的建造队列，也可以通过使用加速道具来减少建造队列所需的时间。")
		: FText::Format(NSLOCTEXT("RokModal", "GenericBuildingDescription", "{0}是城市发展体系中的重要建筑。当前等级为{1}级，可通过升级提升其战力与功能。"), State.DisplayName, FText::AsNumber(State.Level)));
}

void URokUnityModalWidget::BuildResourceBuildingInfo(int32 Type, int32 KnownLevel)
{
	TArray<FString> Headers; TArray<FRokBuildingLevelDetailsRow> Rows; FString Error, Name, Description;
	if (!RokBuildingLevelDetails::Load(Type, Headers, Rows, Error, &Name, &Description))
	{
		UE_LOG(LogTemp, Error, TEXT("%s"), *Error);
		const FText StatusMessage = NSLOCTEXT("RokModal", "BuildingLevelMissing", "建筑等级配置缺失");
		if (!TryMountStatusMessage(ModeCanvas, TEXT("BuildingLevelDetailsMissing"), TEXT("BuildingLevelError"), StatusMessage))
		{
			auto* Missing = MakeModalText(TEXT("BuildingLevelDetailsMissing"), 22, FLinearColor::Red);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(ModeCanvas, Missing, FVector2D(400, 350), FVector2D(650, 60));
		}
		return;
	}
	// The description and level pages, the back button and the column headers live
	// in the page WBP; each level is a row WBP in the scrolling table.
	URokUiPageWidget* Page = MountModePage(TEXT("ResourceBuildingInfo"));
	if (!Page)
	{
		return;
	}
	BuildingDescriptionRoot = Page->Get<UCanvasPanel>(TEXT("BuildingDescriptionRoot"));
	BuildingLevelDetailsRoot = Page->Get<UCanvasPanel>(TEXT("BuildingLevelDetailsRoot"));
	if (BuildingDescriptionRoot)
	{
		AddBuildingPreview(BuildingDescriptionRoot, TEXT("BuildingInfoImage"), Type,
			FVector2D(208.8f, 166.9f), FVector2D(450.0f, 620.2f), FVector2D(438.8f, 507.0f), TEXT("ResourceBuildingInfoPreviewSlot"));
	}
	for (int32 Column = 1; Column < 4; ++Column)
	{
		SetPageText(Page, IndexedName(TEXT("BuildingInfoStatLabel"), Column), FText::FromString(Headers[Column]));
		// Current production and capacity depend on buffs/server state, unlike static level rows.
		SetPageText(Page, IndexedName(TEXT("BuildingInfoStatValue"), Column),
			Column == 3 && Rows.IsValidIndex(KnownLevel - 1) ? FText::AsNumber(Rows[KnownLevel - 1].Power) : FText::FromString(TEXT("—")));
	}
	SetPageText(Page, TEXT("BuildingInfoDescription"), FText::FromString(Description));
	if (UButton* Info = Page->Get<UButton>(TEXT("BuildingLevelInfoButton")))
	{
		Info->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleBuildingLevelInfoClicked);
	}
	BuildingLevelBackButton = Page->Get<UButton>(TEXT("BuildingLevelBackButton"));
	if (BuildingLevelBackButton)
	{
		BuildingLevelBackButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleBuildingLevelBackClicked);
	}

	for (int32 Column = 0; Column < 4; ++Column)
	{
		SetPageText(Page, IndexedName(TEXT("BuildingLevelHeader"), Column), FText::FromString(Headers[Column]));
	}
	BuildingLevelDetailsScroll = Page->Get<UScrollBox>(TEXT("BuildingLevelDetailsScroll"));
	UCanvasPanel* Grid = Page->Get<UCanvasPanel>(TEXT("BuildingLevelRows"));
	for (const auto& Row : Rows)
	{
		const FString Suffix = FString::Printf(TEXT("_%d"), Row.Level);
		URokUiPageWidget* Item = MountItem(TEXT("BuildingLevelRow"), Grid, *(TEXT("BuildingLevelRow") + Suffix),
			FVector2D(0.0f, (Row.Level - 1) * 50.0f), FVector2D(1024.0f, 50.0f), 0, TEXT("BuildingLevel"), Suffix);
		if (!Item)
		{
			break;
		}
		Item->SetPresent(Item->Get<UWidget>(TEXT("CurrentRow")), KnownLevel == Row.Level);
		const int32 Values[4] = { Row.Level, Row.ProduceSpeed, Row.GatherMax, Row.Power };
		for (int32 Column = 0; Column < 4; ++Column)
		{
			SetPageText(Item, IndexedName(TEXT("Cell"), Column), FText::AsNumber(Values[Column]));
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

namespace
{
struct FUpgradeStatRow
{
	FText Label;
	int32 Current = 0;
	int32 Next = 0;
};

TArray<FUpgradeStatRow> BuildingUpgradeStats(const FRokBuildingUpgradePreview& Preview, UGameInstance* GameInstance)
{
	TArray<FUpgradeStatRow> Rows;
	const int32 Type = Preview.CurrentState.UnityBuildingTypeId;
	auto* Catalog = GameInstance ? GameInstance->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
	FRokUnityCatalogBuildingType BuildingType;
	if (Type >= 9 && Type <= 12 && Catalog && Catalog->GetBuildingType(Type, BuildingType))
	{
		const FText Labels[] = {
			NSLOCTEXT("RokModal", "InfantryTrainingCapacity", "步兵单位训练容量"),
			NSLOCTEXT("RokModal", "CavalryTrainingCapacity", "骑兵单位训练容量"),
			NSLOCTEXT("RokModal", "ArcherTrainingCapacity", "弓兵单位训练容量"),
			NSLOCTEXT("RokModal", "SiegeTrainingCapacity", "攻城单位训练容量")
		};
		Rows.Add({Labels[Type - 9], BuildingType.GetTrainingCapacity(Preview.CurrentState.Level),
			BuildingType.GetTrainingCapacity(Preview.TargetLevel)});
	}
	else if (Preview.CurrentProducePerHour > 0 || Preview.NextProducePerHour > 0)
	{
		Rows.Add({NSLOCTEXT("RokModal", "ProductionPerHour", "每小时产量"),
			Preview.CurrentProducePerHour, Preview.NextProducePerHour});
		Rows.Add({NSLOCTEXT("RokModal", "ProductionCapacity", "最大收取量"), Preview.CurrentGatherMax, Preview.NextGatherMax});
	}
	Rows.Add({NSLOCTEXT("RokModal", "UpgradePower", "战力"), Preview.CurrentPower, Preview.NextPower});
	return Rows;
}

FText UpgradeStatDelta(const FUpgradeStatRow& Row)
{
	return FText::FromString(FString::Printf(TEXT("+%s"),
		*FormatNumber(FMath::Max(0, Row.Next - Row.Current)).ToString()));
}

struct FUpgradeCostVisual
{
	int32 Cost;
	FIntRect IconRect;
};

TArray<FUpgradeCostVisual> BuildingUpgradeCosts(const FRokBuildingUpgradePreview& Preview)
{
	return {
		{Preview.Cost.Food, FoodCurrencyRect},
		{Preview.Cost.Wood, WoodCurrencyRect},
		{Preview.Cost.Stone, StoneCurrencyRect},
		{Preview.Cost.Gold, GoldCurrencyRect},
	};
}
}

void URokUnityModalWidget::BuildBuildingUpgrade()
{
	FRokBuildingUpgradePreview Preview;
	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetUpgradeSubsystem();
	const bool bIsSourcePreview = !SelectedBuildingActor.IsValid();
	const bool bHasRuntimePreview = !bIsSourcePreview
		&& UpgradeSubsystem
		&& UpgradeSubsystem->GetUpgradePreview(SelectedBuildingActor.Get(), Preview);
	++UpgradeViewSerial;
	bServerUpgradePending = false;
	ConfirmedInstantQuote.Reset();
	bUpgradeFeedbackFromAction = false;
	bUpgradeUsesServer = !bIsSourcePreview && UpgradeSubsystem && UpgradeSubsystem->UsesServerCity();
	if (bIsSourcePreview)
	{
		// Source-derived farm level-1 -> level-2 preview used by the screenshot
		// command-line acceptance path. It is not persisted as player state.
		Preview.CurrentState.DisplayName = NSLOCTEXT("RokModal", "FarmPreviewName", "农场");
		Preview.CurrentState.Type = ERokBuildingType::Farm;
		Preview.CurrentState.UnityBuildingTypeId = 2;
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

	DisplayedUpgradeLevel = Preview.CurrentState.Level;
	const bool bDeclaredBarracksPreview = !bIsSourcePreview && Preview.CurrentState.UnityBuildingTypeId == 9;
	const bool bBarracksAttributes = bDeclaredBarracksPreview && URokBarracksUpgradeAttributes::Supports(
		Preview.CurrentState.UnityBuildingTypeId, Preview.CurrentState.Level, Preview.TargetLevel);
	if (!URokUiSettings::IsLegacyForced())
	{
		const FName PageName = bBarracksAttributes ? TEXT("BarracksUpgradeSource")
			: (bDeclaredBarracksPreview ? TEXT("BarracksUpgrade") : TEXT("BuildingUpgrade"));
		// Both barracks pages need the row asset. Keep the legacy layout intact if
		// that dependency is unavailable, so the info button still opens a full table.
		UClass* RowClass = bDeclaredBarracksPreview
			? URokUiSettings::ResolvePage(GetDefault<URokUiSettings>()->ModalPages, TEXT("BarracksLevelRow"))
			: nullptr;
		const bool bCanMountPage = !bDeclaredBarracksPreview || (RowClass && !RowClass->HasAnyClassFlags(CLASS_Abstract));
		if (URokUiPageWidget* Page = bCanMountPage ? MountModePage(PageName) : nullptr)
		{
			BindBuildingUpgradePage(Page, Preview, bIsSourcePreview, bHasRuntimePreview,
				bDeclaredBarracksPreview, bBarracksAttributes);
			return;
		}
	}

	UCanvasPanel* UpgradeContentCanvas = ModeCanvas;
	if (bDeclaredBarracksPreview)
	{
		BuildingDescriptionRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("BarracksLevelDescriptionRoot"));
		PlaceOnCanvas(ModeCanvas, BuildingDescriptionRoot, FVector2D::ZeroVector, FVector2D(1440, 900), 1);
		UpgradeContentCanvas = BuildingDescriptionRoot;
	}
	const FVector2D PreviewPosition = bDeclaredBarracksPreview ? FVector2D(203.0f, 110.3f) : FVector2D(203.9f, 166.9f);
	const FVector2D PreviewSize(450.0f, 620.0f);
	PlaceOnCanvas(UpgradeContentCanvas, MakeAtlasBorder(TEXT("BuildingUpgradePreviewFrame"), CommonAtlasPath,
		PreviewFrameRect, CommonAtlasSize, PreviewSize, PreviewFrameMargin), PreviewPosition, PreviewSize);
	AddBuildingPreview(UpgradeContentCanvas, TEXT("BuildingUpgradeImage"), Preview.CurrentState.UnityBuildingTypeId,
		PreviewPosition, PreviewSize, bDeclaredBarracksPreview ? FVector2D(428.0f, 462.0f) : FVector2D(428.9f, 507.0f),
		bDeclaredBarracksPreview ? TEXT("BarracksUpgradePreviewSlot") : TEXT("BuildingUpgradePreviewSlot"));
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
		auto* Attributes = WidgetTree->ConstructWidget<URokBarracksUpgradeAttributes>(URokBarracksUpgradeAttributes::ResolveAttributesClass(), TEXT("BarracksUpgradeAttributes"));
		if (Attributes->InitializeAttributes())
		{
			PlaceOnCanvas(UpgradeContentCanvas, Attributes, URokBarracksUpgradeAttributes::SourcePosition(), FVector2D(536, Attributes->GetSourceHeight()), 2);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Barracks upgrade attributes: %s"), *Attributes->GetLoadError());
			AddBarracksAttributeUnavailable(UpgradeContentCanvas, URokBarracksUpgradeAttributes::SourcePosition(), FVector2D(536, 110), 2);
		}
	}
	else
	{
		const TArray<FUpgradeStatRow> Rows = BuildingUpgradeStats(Preview, GetGameInstance());
		for (int32 Index = 0; Index < Rows.Num(); ++Index)
		{
			const float Y = 263.0f + Index * 54.0f;
			UTextBlock* Label = MakeModalText(*FString::Printf(TEXT("UpgradeStatLabel_%d"), Index), 20, FLinearColor(0.33f, 0.29f, 0.23f, 1.0f));
			Label->SetText(Rows[Index].Label);
			PlaceOnCanvas(UpgradeContentCanvas, Label, FVector2D(700.0f, Y), FVector2D(270.0f, 36.0f), 2);
			UTextBlock* Current = MakeModalText(*FString::Printf(TEXT("UpgradeStatCurrent_%d"), Index), 20, FLinearColor(0.25f, 0.22f, 0.17f, 1.0f), true, ETextJustify::Right);
			Current->SetText(FormatNumber(Rows[Index].Current));
			PlaceOnCanvas(UpgradeContentCanvas, Current, FVector2D(955.0f, Y), FVector2D(90.0f, 36.0f), 2);
			UTextBlock* Delta = MakeModalText(*FString::Printf(TEXT("UpgradeStatDelta_%d"), Index), 20, FLinearColor(0.15f, 0.65f, 0.16f, 1.0f), true);
			Delta->SetText(UpgradeStatDelta(Rows[Index]));
			PlaceOnCanvas(UpgradeContentCanvas, Delta, FVector2D(1060.0f, Y), FVector2D(120.0f, 36.0f), 2);
		}
	
	}

	PlaceOnCanvas(UpgradeContentCanvas, MakeAtlasBorder(TEXT("UpgradeResourceFrame"), CommonAtlasPath, CommonPanelRect, CommonAtlasSize, FVector2D(536.0f, 68.0f), CommonPanelMargin), FVector2D(670.0f, 488.0f), FVector2D(536.0f, 68.0f));
	// UI_Win_BuildingUpdate: the "需求" caption sits above the cost row.
	UTextBlock* RequirementLabel = MakeModalText(
		TEXT("UpgradeRequirementLabel"),
		19,
		FLinearColor(0.88f, 0.86f, 0.80f, 1.0f),
		true);
	RequirementLabel->SetText(NSLOCTEXT("RokModal", "UpgradeRequirements", "需求"));
	PlaceOnCanvas(UpgradeContentCanvas, RequirementLabel, FVector2D(690.0f, 452.0f), FVector2D(160.0f, 30.0f), 3);
	int32 CostVisualIndex = 0;
	for (const FUpgradeCostVisual& Cost : BuildingUpgradeCosts(Preview))
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

	const FText InstantPrice = bIsSourcePreview ? FText::AsNumber(1)
		: NSLOCTEXT("RokModal", "InstantPricePending", "费用待同步");
	UButton* InstantButton = MakeTextAtlasButton(TEXT("InstantUpgradeButton"), CommonAtlasPath, InstantUpgradeButtonRect, CommonAtlasSize, FVector2D(274.0f, 84.0f), NSLOCTEXT("RokModal", "InstantUpgrade", "立即升级"), 26, FLinearColor::White, InstantPrice);
	InstantButton->OnClicked.AddDynamic(this, &URokUnityModalWidget::HandleInstantUpgrade);
	PlaceOnCanvas(UpgradeContentCanvas, InstantButton, FVector2D(660.0f, 650.0f), FVector2D(274.0f, 84.0f), 3);
	if (bIsSourcePreview)
	{
		PlaceOnCanvas(UpgradeContentCanvas, MakeAtlasImage(TEXT("InstantUpgradeGem"), CommonAtlasPath, GemCurrencyRect, CommonAtlasSize, FVector2D(28.0f)), FVector2D(768.0f, 691.0f), FVector2D(28.0f), 5);
	}

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
	FeedbackText->SetWrapTextAt(536.0f);
	PlaceOnCanvas(UpgradeContentCanvas, FeedbackText, FVector2D(670.0f, 592.0f), FVector2D(536.0f, 56.0f), 4);
	if (!bIsSourcePreview) RefreshUpgradeButtons();
	if (bDeclaredBarracksPreview)
	{
		BuildBarracksLevelDetails(Preview.CurrentState.Level);
	}
}

void URokUnityModalWidget::BindBuildingUpgradePage(URokUiPageWidget* Page, const FRokBuildingUpgradePreview& Preview,
	bool bIsSourcePreview, bool bHasRuntimePreview, bool bBarracks, bool bBarracksAttributes)
{
	UCanvasPanel* Content = Page->Get<UCanvasPanel>(bBarracks ? TEXT("BarracksLevelDescriptionRoot") : TEXT("PageRoot"));
	if (bBarracks)
	{
		BuildingDescriptionRoot = Content;
		if (UButton* Info = Page->Get<UButton>(TEXT("BarracksLevelInfoButton")))
		{
			Info->OnClicked.AddUniqueDynamic(this, &URokUnityModalWidget::HandleBarracksLevelInfoClicked);
		}
	}
	if (Content)
	{
		const FVector2D PreviewPosition = bBarracks ? FVector2D(203.0f, 110.3f) : FVector2D(203.9f, 166.9f);
		AddBuildingPreview(Content, TEXT("BuildingUpgradeImage"), Preview.CurrentState.UnityBuildingTypeId,
			PreviewPosition, FVector2D(450.0f, 620.0f), bBarracks ? FVector2D(428.0f, 462.0f) : FVector2D(428.9f, 507.0f),
			bBarracks ? TEXT("BarracksUpgradePreviewSlot") : TEXT("BuildingUpgradePreviewSlot"));
	}
	const FText CurrentLevel = FText::AsNumber(Preview.CurrentState.Level);
	const FText NextLevel = FText::AsNumber(Preview.TargetLevel);
	if (bBarracksAttributes)
	{
		FString GlyphError;
		auto* SourceTextData = NewObject<URokBarracksSourceTextData>(this);
		if (!SourceTextData->Load(GlyphError))
		{
			UE_LOG(LogTemp, Error, TEXT("Barracks source level glyphs: %s"), *GlyphError);
		}
		for (const bool bNext : {false, true})
		{
			if (auto* Text = Page->Get<URokBarracksSourceTextWidget>(bNext ? TEXT("UpgradeNextLevel") : TEXT("UpgradeCurrentLevel")))
			{
				Text->Configure(SourceTextData, 24, true, 3, URokBarracksUpgradeAttributes::LevelSize(bNext), FLinearColor::White);
				if (!Text->SetText(bNext ? NextLevel : FText::Format(NSLOCTEXT("RokModal", "BarracksSourceLevel", "等级{0}"), CurrentLevel)))
				{
					UE_LOG(LogTemp, Error, TEXT("Barracks source level glyph missing for %s"), *Text->GetText().ToString());
				}
			}
		}
		UCanvasPanel* Placeholder = Page->Get<UCanvasPanel>(TEXT("BarracksUpgradeAttributesSlot"));
		if (const auto* AttributeSlot = Placeholder ? Cast<UCanvasPanelSlot>(Placeholder->Slot) : nullptr)
		{
			const FVector2D Position = AttributeSlot->GetPosition();
			const FVector2D Size = AttributeSlot->GetSize();
			const int32 ZOrder = AttributeSlot->GetZOrder();
			auto* Attributes = WidgetTree->ConstructWidget<URokBarracksUpgradeAttributes>(
				URokBarracksUpgradeAttributes::ResolveAttributesClass(), TEXT("BarracksUpgradeAttributes"));
			// Construct the source-backed Slate content before adding it to the live page.
			if (Attributes->InitializeAttributes())
			{
				PlaceOnCanvas(Content, Attributes, Position, Size, ZOrder);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Barracks upgrade attributes: %s"), *Attributes->GetLoadError());
				AddBarracksAttributeUnavailable(Content, Position, Size, ZOrder);
			}
		}
	}
	else
	{
		SetPageText(Page, TEXT("UpgradeCurrentLevel"), CurrentLevel);
		SetPageText(Page, TEXT("UpgradeNextLevel"), NextLevel);
		const TArray<FUpgradeStatRow> Rows = BuildingUpgradeStats(Preview, GetGameInstance());
		for (int32 Index = 0; Index < 3; ++Index)
		{
			UTextBlock* Label = Page->Get<UTextBlock>(IndexedName(TEXT("UpgradeStatLabel"), Index));
			UTextBlock* Current = Page->Get<UTextBlock>(IndexedName(TEXT("UpgradeStatCurrent"), Index));
			UTextBlock* Delta = Page->Get<UTextBlock>(IndexedName(TEXT("UpgradeStatDelta"), Index));
			const bool bShow = Rows.IsValidIndex(Index);
			Page->SetShown(Label, bShow);
			Page->SetShown(Current, bShow);
			Page->SetShown(Delta, bShow);
			if (bShow)
			{
				if (Label) Label->SetText(Rows[Index].Label);
				if (Current) Current->SetText(FormatNumber(Rows[Index].Current));
				if (Delta) Delta->SetText(UpgradeStatDelta(Rows[Index]));
			}
		}
	}

	int32 CostIndex = 0;
	const TArray<FUpgradeCostVisual> Costs = BuildingUpgradeCosts(Preview);
	for (const FUpgradeCostVisual& Cost : Costs)
	{
		if (Cost.Cost > 0)
		{
			UImage* Icon = Page->Get<UImage>(IndexedName(TEXT("UpgradeCostIcon"), CostIndex));
			Page->SetShown(Icon, true);
			SetAtlasSprite(Icon, CommonAtlasPath, Cost.IconRect, CommonAtlasSize, FVector2D(46));
			SetPageText(Page, IndexedName(TEXT("UpgradeCostText"), CostIndex), FormatNumber(Cost.Cost));
			Page->SetShown(Page->Get<UTextBlock>(IndexedName(TEXT("UpgradeCostText"), CostIndex)), true);
			++CostIndex;
		}
	}
	for (; CostIndex < Costs.Num(); ++CostIndex)
	{
		Page->SetShown(Page->Get<UImage>(IndexedName(TEXT("UpgradeCostIcon"), CostIndex)), false);
		Page->SetShown(Page->Get<UTextBlock>(IndexedName(TEXT("UpgradeCostText"), CostIndex)), false);
	}

	SetPageText(Page, TEXT("InstantUpgradeButtonSecondary"), bIsSourcePreview ? FText::AsNumber(1)
		: NSLOCTEXT("RokModal", "InstantPricePending", "费用待同步"));
	SetPageText(Page, TEXT("TimedUpgradeButtonSecondary"), FText::Format(
		NSLOCTEXT("RokModal", "Seconds", "{0}秒"), FText::AsNumber(Preview.BuildSeconds)));
	if (!bBarracks)
	{
		UImage* Gem = Page->Get<UImage>(TEXT("InstantUpgradeGem"));
		Page->SetShown(Gem, bIsSourcePreview);
		if (bIsSourcePreview)
		{
			SetAtlasSprite(Gem, CommonAtlasPath, GemCurrencyRect, CommonAtlasSize, FVector2D(28));
		}
	}
	if (UButton* Instant = Page->Get<UButton>(TEXT("InstantUpgradeButton")))
	{
		Instant->OnClicked.AddUniqueDynamic(this, &URokUnityModalWidget::HandleInstantUpgrade);
	}
	if (UButton* Timed = Page->Get<UButton>(TEXT("TimedUpgradeButton")))
	{
		Timed->SetIsEnabled(bIsSourcePreview || (bHasRuntimePreview && Preview.bCanStartTimedUpgrade));
		Timed->OnClicked.AddUniqueDynamic(this, &URokUnityModalWidget::HandleStartUpgrade);
	}
	FeedbackText = Page->Get<UTextBlock>(TEXT("UpgradeFeedback"));
	if (FeedbackText)
	{
		const bool bShow = !bIsSourcePreview && bHasRuntimePreview
			&& !Preview.bCanStartTimedUpgrade && !Preview.FailureReason.IsEmpty();
		FeedbackText->SetText(bShow ? FText::FromString(Preview.FailureReason) : FText::GetEmpty());
		FeedbackText->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!bIsSourcePreview)
	{
		RefreshUpgradeButtons();
	}
	if (bBarracks)
	{
		BuildBarracksLevelDetails(Preview.CurrentState.Level, Page);
	}
}

void URokUnityModalWidget::BuildBarracksLevelDetails(int32 KnownLevel, URokUiPageWidget* Page)
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
		if (auto* InfoButton = Cast<UButton>(RokUiFind::Find(this, TEXT("BarracksLevelInfoButton"))))
		{
			InfoButton->SetIsEnabled(false);
		}
		const FText StatusMessage = NSLOCTEXT("RokModal", "BarracksLevelHeaderSourceUnavailable", "兵营等级表头源字形不可用");
		if (!TryMountStatusMessage(BuildingDescriptionRoot, TEXT("BarracksLevelHeaderSourceUnavailable"), TEXT("BarracksHeaderError"), StatusMessage))
		{
			auto* Missing = MakeModalText(TEXT("BarracksLevelHeaderSourceUnavailable"), 15, FLinearColor::Red);
			Missing->SetText(StatusMessage);
			PlaceOnCanvas(BuildingDescriptionRoot, Missing, FVector2D(700, 570), FVector2D(480, 30), 5);
		}
		return;
	}
	for (int32 Column = 0; Column < 4; ++Column)
	{
		if (!HeaderSourceData->Find(Headers[Column], 22, false, 4, 250))
		{
			UE_LOG(LogTemp, Error, TEXT("Barracks level header source glyph missing: column=%d text=%s"), Column, *Headers[Column]);
			if (auto* InfoButton = Cast<UButton>(RokUiFind::Find(this, TEXT("BarracksLevelInfoButton"))))
			{
				InfoButton->SetIsEnabled(false);
			}
			const FText StatusMessage = NSLOCTEXT("RokModal", "BarracksLevelHeaderSourceUnavailable", "兵营等级表头源字形不可用");
			if (!TryMountStatusMessage(BuildingDescriptionRoot, TEXT("BarracksLevelHeaderSourceUnavailable"), TEXT("BarracksHeaderError"), StatusMessage))
			{
				auto* Missing = MakeModalText(TEXT("BarracksLevelHeaderSourceUnavailable"), 15, FLinearColor::Red);
				Missing->SetText(StatusMessage);
				PlaceOnCanvas(BuildingDescriptionRoot, Missing, FVector2D(700, 570), FVector2D(480, 30), 5);
			}
			return;
		}
	}
	BarracksLevelSelectedLevel = Rows.IsValidIndex(KnownLevel - 1) ? KnownLevel : 0;
	if (Page)
	{
		BarracksLevelDetailsRoot = Page->Get<UCanvasPanel>(TEXT("BarracksLevelDetailsRoot"));
		BarracksLevelDetailsScroll = Page->Get<UScrollBox>(TEXT("BarracksLevelDetailsScroll"));
		BarracksLevelBackButton = Page->Get<UButton>(TEXT("BarracksLevelBackButton"));
		if (BarracksLevelBackButton)
		{
			BarracksLevelBackButton->OnClicked.AddUniqueDynamic(this, &URokUnityModalWidget::HandleBarracksLevelBackClicked);
			// The back button sits above the shared window header, which is mounted outside this page.
			if (const auto* BackSlot = Cast<UCanvasPanelSlot>(BarracksLevelBackButton->Slot))
			{
				const FVector2D Position = BackSlot->GetPosition();
				const FVector2D Size = BackSlot->GetSize();
				const int32 ZOrder = BackSlot->GetZOrder();
				BarracksLevelBackButton->RemoveFromParent();
				PlaceOnCanvas(ModeCanvas, BarracksLevelBackButton, Position, Size, ZOrder);
			}
		}
		for (int32 Column = 0; Column < Headers.Num(); ++Column)
		{
			if (auto* Header = Page->Get<URokBarracksSourceTextWidget>(IndexedName(TEXT("BarracksLevelHeader"), Column)))
			{
				Header->Configure(HeaderSourceData, 22, false, 4, FVector2D(250, 50), FLinearColor::White);
				Header->SetText(FText::FromString(Headers[Column]));
			}
		}
		if (USizeBox* Size = Page->Get<USizeBox>(TEXT("BarracksLevelRowsSize")))
		{
			Size->SetHeightOverride(Rows.Num() * 50.0f);
		}
		UCanvasPanel* Grid = Page->Get<UCanvasPanel>(TEXT("BarracksLevelRows"));
		for (const FRokBarracksLevelDetailsRow& Row : Rows)
		{
			const FString Suffix = FString::Printf(TEXT("_%d"), Row.Level);
			URokUiPageWidget* Item = MountItem(TEXT("BarracksLevelRow"), Grid, *(TEXT("BarracksLevelRow") + Suffix),
				FVector2D(0.0f, (Row.Level - 1) * 50.0f), FVector2D(1024.0f, 50.0f), 0, TEXT("BarracksLevel"), Suffix);
			if (!Item)
			{
				break;
			}
			Item->SetPresent(Item->Get<UWidget>(TEXT("CurrentRow")), KnownLevel == Row.Level);
			const FText Values[] = {FText::AsNumber(Row.Level), FText::AsNumber(Row.InfantryTrainNumber),
				FText::FromString(Row.InfantryAttackText), FText::AsNumber(Row.Power)};
			for (int32 Column = 0; Column < UE_ARRAY_COUNT(Values); ++Column)
			{
				SetPageText(Item, IndexedName(TEXT("Cell"), Column), Values[Column]);
			}
		}
		ApplyBarracksLevelPageAnimation();
		return;
	}
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
	UWidget* Anchor = RokUiFind::Find(this, TEXT("ProfileKillsHelpButton"));
	const auto* AnchorSlot = Anchor ? Cast<UCanvasPanelSlot>(Anchor->Slot) : nullptr;
	if (!AnchorSlot) return;
	const FVector2D AnchorCenter = AnchorSlot->GetPosition() + AnchorSlot->GetSize() * 0.5;
	using namespace RokPlayerKillTipAnimation;
	const float Left = FMath::Clamp(float(AnchorCenter.X - Width * 0.5), 0.0f,
		FMath::Max(0.0f, float(CurrentExpandedDesignSize.X) - Width));
	const float Top = AnchorCenter.Y + 12.0f;

	// The click-away layer and the bubble are an item WBP over the profile page;
	// C++ places the bubble under the help button and points the arrow at it.
	URokUiPageWidget* Tip = MountTipLayer(TEXT("PlayerKillTip"), TEXT("PlayerKillTipLayer"));
	PlayerKillTipLayer = Tip;
	PlayerKillTipRoot = Tip ? Tip->Get<UCanvasPanel>(TEXT("PlayerKillTipRoot")) : nullptr;
	if (!PlayerKillTipRoot)
	{
		ClosePlayerKillTip();
		return;
	}
	CastChecked<UCanvasPanelSlot>(PlayerKillTipRoot->Slot)->SetPosition(FVector2D(Left, Top));
	PlayerKillTipRoot->SetRenderTransformPivot(FVector2D(0.5));
	PlayerKillTipRoot->SetRenderScale(FVector2D(0));
	if (UButton* Mask = Tip->Get<UButton>(TEXT("PlayerKillTipClickAway")))
	{
		Mask->OnClicked.AddDynamic(this, &URokUnityModalWidget::ClosePlayerKillTip);
	}
	if (UWidget* Arrow = Tip->Get<UWidget>(TEXT("PlayerKillTipArrow")))
	{
		auto* ArrowSlot = CastChecked<UCanvasPanelSlot>(Arrow->Slot);
		ArrowSlot->SetPosition(FVector2D(AnchorCenter.X - Left - 7.5f, ArrowSlot->GetPosition().Y));
	}
	// Kill statistics are server state that is not synchronized: never a fabricated zero.
	SetPageText(Tip, TEXT("PlayerKillTipTotal"), NSLOCTEXT("RokModal", "PlayerKillTotalUnknown", "击杀总数：—（待同步）"));
	for (int32 Tier = 1; Tier <= 5; ++Tier)
	{
		SetPageText(Tip, *FString::Printf(TEXT("PlayerKillTipTierCount_%d"), Tier), NSLOCTEXT("RokModal", "PlayerKillTierUnknown", "—"));
	}
	UE_LOG(LogTemp, Display, TEXT("Rok player kill tooltip opened: source=UI_KillTip statistics=server_unknown"));
}

void URokUnityModalWidget::HandleProfileActionHelpClicked()
{
	if (ActiveMode != ERokUnityModalMode::PlayerProfile || !ModeCanvas) return;
	// PlayerDataMediator.ShowPowerAction -> HelpTip arrowUp; keep profile underneath.
	ClosePlayerPowerActionTip();
	ClosePlayerKillTip();
	UWidget* Anchor = RokUiFind::Find(this, TEXT("ProfileActionHelpButton"));
	const auto* AnchorSlot = Anchor ? Cast<UCanvasPanelSlot>(Anchor->Slot) : nullptr;
	if (!AnchorSlot) return;
	const FVector2D AnchorCenter = AnchorSlot->GetPosition() + AnchorSlot->GetSize() * 0.5;
	using namespace RokPowerActionTip;
	const float Left = FMath::Clamp(float(AnchorCenter.X - Width * 0.5), 0.0f,
		FMath::Max(0.0f, float(CurrentExpandedDesignSize.X) - Width));
	const float Top = AnchorCenter.Y + 12.0f;

	URokUiPageWidget* Tip = MountTipLayer(TEXT("PlayerActionTip"), TEXT("PlayerActionTipLayer"));
	PlayerPowerActionTipLayer = Tip;
	PlayerPowerActionTipRoot = Tip ? Tip->Get<UCanvasPanel>(TEXT("PlayerActionTipRoot")) : nullptr;
	if (!PlayerPowerActionTipRoot)
	{
		ClosePlayerPowerActionTip();
		return;
	}
	CastChecked<UCanvasPanelSlot>(PlayerPowerActionTipRoot->Slot)->SetPosition(FVector2D(Left, Top));
	PlayerPowerActionTipRoot->SetRenderTransformPivot(FVector2D(0.5));
	PlayerPowerActionTipRoot->SetRenderScale(FVector2D(0));
	if (UButton* Mask = Tip->Get<UButton>(TEXT("PlayerActionTipClickAway")))
	{
		Mask->OnClicked.AddDynamic(this, &URokUnityModalWidget::ClosePlayerPowerActionTip);
	}
	if (UWidget* Arrow = Tip->Get<UWidget>(TEXT("PlayerActionTipArrow")))
	{
		auto* ArrowSlot = CastChecked<UCanvasPanelSlot>(Arrow->Slot);
		ArrowSlot->SetPosition(FVector2D(AnchorCenter.X - Left - 7.5f, ArrowSlot->GetPosition().Y));
	}
	if (auto* Recovery = Tip->Get<URichTextBlock>(TEXT("PlayerActionTipRecovery")))
	{
		// The rich-text styles are built at runtime from the engine default font
		// (a WBP cannot hold this transient style table).
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
		// Source 100708 needs city attributes, lastActionForceTime and server time.
		// Keep every unavailable term unknown; there is no synthetic recovery timer.
		Recovery->SetText(NSLOCTEXT("RokModal", "PowerActionTipRecoveryUnknown",
			"行动力恢复1点需要—秒\n（恢复速度提升：+ —%）\n\n全部恢复还需   <Remaining>—</>"));
	}
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

void URokUnityModalWidget::RefreshUpgradeButtons()
{
	UpgradeButtonRefreshRemaining = 0.2f;
	URokBuildingUpgradeSubsystem* Subsystem = GetUpgradeSubsystem();
	AActor* Building = SelectedBuildingActor.Get();
	FString TimedFailure, InstantFailure;
	const bool bCanEvaluate = Subsystem && Building && !bServerUpgradePending;
	int64 Quote = -1;
	bool bTimedEnabled = false;
	bool bInstantEnabled = false;
	if (bCanEvaluate && bUpgradeUsesServer)
	{
		bTimedEnabled = Subsystem->CanStartServerUpgrade(Building, false, TimedFailure, DisplayedUpgradeLevel);
		int64 Estimate = 0;
		if (Subsystem->GetServerInstantUpgradeQuote(Building, Estimate)) Quote = Estimate;
		if (Subsystem->CanStartServerUpgrade(Building, true, InstantFailure, DisplayedUpgradeLevel))
		{
			bInstantEnabled = Quote >= 0;
			if (!bInstantEnabled) InstantFailure = TEXT("立即升级费用尚未同步。");
		}
	}
	else if (bCanEvaluate)
	{
		bInstantEnabled = Subsystem->CanStartLocalInstantUpgrade(Building, DisplayedUpgradeLevel, Quote, InstantFailure);
		if (Subsystem->GetBuildingStateForActor(Building).Level == DisplayedUpgradeLevel)
		{
			bTimedEnabled = Subsystem->CanStartUpgrade(Building, TimedFailure);
		}
		else
		{
			TimedFailure = TEXT("建筑等级已变化，请重新打开升级界面。");
		}
	}
	if (auto* Price = Cast<UTextBlock>(RokUiFind::Find(this, TEXT("InstantUpgradeButtonSecondary"))))
	{
		const FText PriceFormat = bUpgradeUsesServer ? NSLOCTEXT("RokModal", "InstantGemEstimate", "约 {0} 宝石")
			: NSLOCTEXT("RokModal", "InstantGemPrice", "{0} 宝石");
		Price->SetText(Quote >= 0 ? FText::Format(PriceFormat, FText::AsNumber(Quote))
			: NSLOCTEXT("RokModal", "InstantPricePending", "费用待同步"));
	}
	if (auto* TimedButton = Cast<UButton>(RokUiFind::Find(this, TEXT("TimedUpgradeButton"))))
	{
		TimedButton->SetIsEnabled(bTimedEnabled);
	}
	if (auto* InstantButton = Cast<UButton>(RokUiFind::Find(this, TEXT("InstantUpgradeButton"))))
	{
		InstantButton->SetIsEnabled(bInstantEnabled);
	}
	if (FeedbackText && !bUpgradeFeedbackFromAction)
	{
		const FString& Failure = !bInstantEnabled && !InstantFailure.IsEmpty() ? InstantFailure : TimedFailure;
		FeedbackText->SetText(FText::FromString(Failure));
		FeedbackText->SetVisibility(Failure.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void URokUnityModalWidget::ShowUpgradeFeedback(const FText& Text)
{
	if (!FeedbackText) return;
	bUpgradeFeedbackFromAction = true;
	FeedbackText->SetText(Text);
	FeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URokUnityModalWidget::SubmitServerUpgrade(bool bImmediately)
{
	URokBuildingUpgradeSubsystem* Subsystem = GetUpgradeSubsystem();
	if (!Subsystem || !SelectedBuildingActor.IsValid() || bServerUpgradePending) return;
	FString Failure;
	const uint32 ViewSerial = UpgradeViewSerial;
	const TWeakObjectPtr<URokUnityModalWidget> WeakThis(this);
	if (!Subsystem->StartServerUpgrade(SelectedBuildingActor.Get(), DisplayedUpgradeLevel, bImmediately,
		[WeakThis, ViewSerial](const FString& Result)
		{
			URokUnityModalWidget* Widget = WeakThis.Get();
			if (!Widget || Widget->ActiveMode != ERokUnityModalMode::BuildingUpgrade
				|| Widget->UpgradeViewSerial != ViewSerial) return;
			Widget->bServerUpgradePending = false;
			if (Result.IsEmpty()) Widget->CloseModal();
			else Widget->ShowUpgradeFeedback(FText::FromString(Result));
		}, Failure))
	{
		ShowUpgradeFeedback(FText::FromString(Failure));
		return;
	}
	bServerUpgradePending = true;
	ShowUpgradeFeedback(NSLOCTEXT("RokModal", "UpgradePending", "正在等待服务器确认升级。"));
	RefreshUpgradeButtons();
}

void URokUnityModalWidget::HandleStartUpgrade()
{
	ConfirmedInstantQuote.Reset();
	if (bUpgradeUsesServer)
	{
		SubmitServerUpgrade(false);
		return;
	}
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

void URokUnityModalWidget::HandleInstantUpgrade()
{
	if (bServerUpgradePending) return;
	auto* Subsystem = GetUpgradeSubsystem();
	AActor* Building = SelectedBuildingActor.Get();
	if (!Subsystem || !Building)
	{
		ShowUpgradeFeedback(NSLOCTEXT("RokModal", "SourcePreviewNoMutation", "配置预览，不会写入玩家状态"));
		return;
	}

	FString Failure;
	int64 Quote = 0;
	const bool bCanUpgrade = bUpgradeUsesServer
		? Subsystem->CanStartServerUpgrade(Building, true, Failure, DisplayedUpgradeLevel)
			&& Subsystem->GetServerInstantUpgradeQuote(Building, Quote)
		: Subsystem->CanStartLocalInstantUpgrade(Building, DisplayedUpgradeLevel, Quote, Failure);
	if (!bCanUpgrade)
	{
		ConfirmedInstantQuote.Reset();
		ShowUpgradeFeedback(Failure.IsEmpty() ? NSLOCTEXT("RokModal", "InstantPricePending", "费用待同步") : FText::FromString(Failure));
		return;
	}
	if (!ConfirmedInstantQuote.IsSet() || ConfirmedInstantQuote.GetValue() != Quote)
	{
		ConfirmedInstantQuote = Quote;
		const FText Format = bUpgradeUsesServer
			? NSLOCTEXT("RokModal", "ConfirmServerInstantUpgradeEstimate", "预计消耗 {0} 宝石，实际费用由服务器结算。再次点击确认。")
			: NSLOCTEXT("RokModal", "ConfirmLocalInstantUpgrade", "消耗所需资源和 {0} 宝石。再次点击确认。");
		ShowUpgradeFeedback(FText::Format(Format, FText::AsNumber(Quote)));
		return;
	}

	ConfirmedInstantQuote.Reset();
	if (bUpgradeUsesServer)
	{
		SubmitServerUpgrade(true);
	}
	else if (Subsystem->StartLocalInstantUpgrade(Building, DisplayedUpgradeLevel, Quote, Failure))
	{
		CloseModal();
	}
	else
	{
		ShowUpgradeFeedback(FText::FromString(Failure));
	}
}
