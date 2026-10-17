#include "RokLanguageSettingsAuditSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/TextWidgetTypes.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokClientSettingsSubsystem.h"
#include "RokMainHudWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/ObjectWriter.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"
#include "Widgets/SWidget.h"

namespace
{
constexpr int32 LanguageAuditCaptureCount = 17;
// Existing action/control identities are retained; visual source order is 1,10,37,40.
constexpr int32 LanguageAuditIds[] = {10, 1, 40, 37};
const FName LanguageAuditButtons[] = {
	TEXT("LanguageOptionEnglishButton"), TEXT("LanguageOptionArabicButton"),
	TEXT("LanguageOptionChineseButton"), TEXT("LanguageOptionTurkishButton")};
bool LanguageAuditVisible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
	for (const UObject* Outer = Widget->GetOuter(); Outer; Outer = Outer->GetOuter())
		if (const auto* Parent = Cast<UUserWidget>(Outer); Parent && (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0)) return false;
	return true;
}
bool LanguageAuditGeometry(const UWidget* Widget, FVector2D Position, FVector2D Size, bool bRendered = true)
{
	const auto* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
	return Slot && Slot->GetPosition().Equals(Position, .02) && Slot->GetSize().Equals(Size, .02)
		&& (!bRendered || Widget->GetCachedGeometry().GetLocalSize().Equals(Size, .1));
}
const FShapedTextOptions* LanguageAuditTextOptions(const UTextBlock* Text)
{
	const auto* Property = Text ? FindFProperty<FStructProperty>(Text->GetClass(), TEXT("ShapedTextOptions")) : nullptr;
	return Property && Property->Struct == FShapedTextOptions::StaticStruct()
		? Property->ContainerPtrToValuePtr<FShapedTextOptions>(Text) : nullptr;
}
bool LanguageAuditAtlas(const UImage* Image, int32 X, int32 Y, int32 Width, int32 Height)
{
	if (!Image || !Image->GetBrush().GetResourceObject()
		|| Image->GetBrush().GetResourceObject()->GetPathName() != TEXT("/Game/RokPrototype/UI/MainHudAtlases/rok_hud_atlas_common.rok_hud_atlas_common")) return false;
	const FBox2f UV = Image->GetBrush().GetUVRegion();
	return UV.Min.Equals(FVector2f(float(X) / 2048, float(1024 - Y - Height) / 1024), .00001f)
		&& UV.Max.Equals(FVector2f(float(X + Width) / 2048, float(1024 - Y) / 1024), .00001f);
}
bool LanguageAuditPreferencePaths(TArray<FString>& Result, TMap<FString, FString>& PathsByKey)
{
	if (!GConfig) return false;
	IFileManager::Get().FindFilesRecursive(Result, *(FPaths::ProjectSavedDir() / TEXT("Config")), TEXT("*.ini"), true, false, false);
	// UE 5.8 globals are known-config cache keys. Only the branch's IniPath is a file.
	for (const FString& Key : TArray<FString>{GGameUserSettingsIni, GGameIni, GEngineIni, GScalabilityIni})
	{
		if (Key.IsEmpty()) return false;
		const auto* Branch = GConfig->FindBranch(FName(*Key), FString());
		if (!Branch || Branch->IniPath.IsEmpty() || !Branch->IniPath.EndsWith(TEXT(".ini"), ESearchCase::IgnoreCase)) return false;
		FString Path = FPaths::ConvertRelativePathToFull(Branch->IniPath); FPaths::NormalizeFilename(Path);
		if (Key == GGameUserSettingsIni && !IFileManager::Get().FileExists(*Path)) return false;
		PathsByKey.Add(Key, Path); Result.Add(Path);
	}
	for (FString& Path : Result) { Path = FPaths::ConvertRelativePathToFull(Path); FPaths::NormalizeFilename(Path); }
	Result.Sort();
	for (int32 Index = Result.Num() - 1; Index > 0; --Index)
		if (FPaths::IsSamePath(Result[Index], Result[Index - 1])) Result.RemoveAt(Index);
	return true;
}
}

bool URokLanguageSettingsAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokLanguageSettingsAudit")) && !IsRunningDedicatedServer()
		&& Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokLanguageSettingsAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokLanguageSettingsAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/LanguageSettingsAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokLanguageSettingsAuditSubsystem::ScreenshotProcessed);
}
void URokLanguageSettingsAuditSubsystem::Deinitialize()
{
	FInternationalization::Get().OnCultureChanged().Remove(CultureChangedHandle);
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (bSnapshot && !bFinished)
	{
		Failures.Add(TEXT("World teardown interrupted the language draft audit; reload is outside its scope."));
		bFinished = true;
		if (!Save(false)) UE_LOG(LogTemp, Error, TEXT("Rok LanguageSettings audit could not save teardown failure."));
		UE_LOG(LogTemp, Error, TEXT("Rok LanguageSettings audit interrupted before completion: %s"), *Directory);
	}
	Super::Deinitialize();
}
TStatId URokLanguageSettingsAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokLanguageSettingsAuditSubsystem, STATGROUP_Tickables); }

bool URokLanguageSettingsAuditSubsystem::CheckIdentity()
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); FPaths::NormalizeDirectoryName(Project);
	if (!FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok")) || !GetWorld()
		|| GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityGameAppCity"))
	{ Finish(TEXT("Wrong project/map; no language-window actions are permitted.")); return false; }
	return true;
}
bool URokLanguageSettingsAuditSubsystem::Begin()
{
	if (!CheckIdentity()) return false;
	const TCHAR* Cursor = FCommandLine::Get(); FString Token;
	while (FParse::Token(Cursor, Token, false))
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
			&& ((Token.Contains(TEXT("Audit"), ESearchCase::IgnoreCase) && !Token.Equals(TEXT("-RokLanguageSettingsAudit"), ESearchCase::IgnoreCase))
				|| Token.StartsWith(TEXT("-RokOpen"), ESearchCase::IgnoreCase) || Token.StartsWith(TEXT("-RokAuto"), ESearchCase::IgnoreCase)))
		{ Finish(TEXT("Use a dedicated process without other audit/open/auto-route flags.")); return false; }
	InitialGameInstance = GetWorld()->GetGameInstance(); InitialController = GetWorld()->GetFirstPlayerController();
	const auto* Settings = InitialGameInstance.IsValid() ? InitialGameInstance->GetSubsystem<URokClientSettingsSubsystem>() : nullptr;
	if (!InitialController.IsValid() || !Settings || !Settings->AreSettingsLoaded())
	{ Finish(TEXT("Local player and settings must finish initialization before the audit baseline.")); return false; }
	TArray<UUserWidget*> Widgets;
	const auto Find = [&](UClass* Class) -> UUserWidget*
	{
		Widgets.Reset(); UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, Class, false);
		UUserWidget* Result = nullptr; int32 Count = 0;
		for (auto* Widget : Widgets) if (Widget->GetWorld() == GetWorld()) { Result = Widget; ++Count; }
		return Count == 1 ? Result : nullptr;
	};
	Hud = Cast<URokMainHudWidget>(Find(URokMainHudWidget::StaticClass()));
	Modal = Cast<URokUnityModalWidget>(Find(URokUnityModalWidget::StaticClass()));
	Feature = Cast<URokUnityFeatureScreenWidget>(Find(URokUnityFeatureScreenWidget::StaticClass()));
	if (!LanguageAuditVisible(Hud.Get()) || !Modal.IsValid() || !Feature.IsValid() || Modal->IsModalOpen() || Feature->IsScreenOpen())
	{ Finish(TEXT("Expected an attached city HUD and closed modal/feature hosts.")); return false; }
	if (!SnapshotPreferences()) { Finish(TEXT("Cannot establish a complete read-only preference baseline.")); return false; }
	return Validate();
}
bool URokLanguageSettingsAuditSubsystem::SnapshotPreferences()
{
	if (GGameUserSettingsIni.IsEmpty()) return false;
	TArray<FString> Paths;
	if (!LanguageAuditPreferencePaths(Paths, PreferencePathsByKey)) return false;
	for (const FString& Path : Paths)
	{
		FPreferenceFile File; File.Path = Path; File.bExists = IFileManager::Get().FileExists(*Path);
		if (File.bExists && !FFileHelper::LoadFileToArray(File.Bytes, *Path)) return false;
		File.Modified = IFileManager::Get().GetTimeStamp(*Path);
		PreferenceFiles.Add(MoveTemp(File));
	}
	if (PreferenceFiles.IsEmpty()) return false;
	FObjectWriter(GetMutableDefault<URokClientSettingsConfig>(), InitialClientSettings, false, false, false);
	InitialLanguage = FInternationalization::Get().GetCurrentLanguage()->GetName();
	InitialLocale = FInternationalization::Get().GetCurrentLocale()->GetName();
	CultureChangedHandle = FInternationalization::Get().OnCultureChanged().AddUObject(this, &URokLanguageSettingsAuditSubsystem::CultureChanged);
	bSnapshot = true;
	return Save(false);
}
void URokLanguageSettingsAuditSubsystem::CultureChanged()
{
	++CultureChangeCount;
	Finish(TEXT("An actual culture-change event occurred during a draft-only language audit."));
}
bool URokLanguageSettingsAuditSubsystem::CheckUnchanged(bool bCheckDisk)
{
	if (!CheckIdentity()) return false;
	if (!bSnapshot || !InitialGameInstance.IsValid() || !InitialController.IsValid()
		|| GetWorld()->GetGameInstance() != InitialGameInstance.Get() || GetWorld()->GetFirstPlayerController() != InitialController.Get()
		|| !GetWorld()->NextURL.IsEmpty())
	{ Finish(TEXT("Runtime identity changed or a map reload was scheduled.")); return false; }
	if (CultureChangeCount != 0 || FInternationalization::Get().GetCurrentLanguage()->GetName() != InitialLanguage
		|| FInternationalization::Get().GetCurrentLocale()->GetName() != InitialLocale)
	{ Finish(TEXT("The engine language or locale changed while only a draft was selected.")); return false; }
	TArray<uint8> CurrentSettings;
	FObjectWriter(GetMutableDefault<URokClientSettingsConfig>(), CurrentSettings, false, false, false);
	if (CurrentSettings != InitialClientSettings) { Finish(TEXT("In-memory client preferences changed during language selection.")); return false; }
	if (!bCheckDisk) return true;
	TArray<FString> CurrentPaths; TMap<FString, FString> CurrentPathsByKey;
	if (!LanguageAuditPreferencePaths(CurrentPaths, CurrentPathsByKey) || CurrentPathsByKey.Num() != PreferencePathsByKey.Num())
	{ Finish(TEXT("Known preference config branches no longer resolve to actual ini files.")); return false; }
	for (const auto& Pair : PreferencePathsByKey)
	{
		const FString* CurrentPath = CurrentPathsByKey.Find(Pair.Key);
		if (!CurrentPath || !FPaths::IsSamePath(*CurrentPath, Pair.Value))
		{ Finish(TEXT("A known preference config branch changed its actual file path.")); return false; }
	}
	if (CurrentPaths.Num() != PreferenceFiles.Num()) { Finish(TEXT("Preference file inventory changed.")); return false; }
	for (int32 Index = 0; Index < PreferenceFiles.Num(); ++Index)
	{
		const auto& File = PreferenceFiles[Index]; TArray<uint8> CurrentBytes;
		if (!FPaths::IsSamePath(CurrentPaths[Index], File.Path) || IFileManager::Get().FileExists(*File.Path) != File.bExists
			|| (File.bExists && (!FFileHelper::LoadFileToArray(CurrentBytes, *File.Path) || CurrentBytes != File.Bytes
				|| IFileManager::Get().GetTimeStamp(*File.Path) != File.Modified)))
		{ Finish(TEXT("Preference file contents or existence changed; values are not recorded.")); return false; }
	}
	++DiskComparisonCount;
	return true;
}

bool URokLanguageSettingsAuditSubsystem::Validate()
{
	if (!CheckUnchanged(true)) return false;
	if (!Hud.IsValid() || !Modal.IsValid() || !Feature.IsValid() || !LanguageAuditVisible(Hud.Get()))
	{ Finish(TEXT("The actual runtime widget hosts were destroyed or hidden.")); return false; }
	if (View == EView::City)
	{
		if (Modal->IsModalOpen() || Feature->IsScreenOpen() || LanguageAuditVisible(Modal.Get()) || LanguageAuditVisible(Feature.Get())
			|| Feature->GetWidgetFromName(TEXT("LanguageConfirmButton")))
		{ Finish(TEXT("Expected city with the language controls removed after closing.")); return false; }
		if (Step > 0) bObservedLanguageRemoval = true;
		return true;
	}
	if (View == EView::Profile)
	{
		if (!Modal->IsModalOpen() || Feature->IsScreenOpen() || !LanguageAuditVisible(Modal->GetWidgetFromName(TEXT("ProfileSettingsButton"))))
		{ Finish(TEXT("Expected the real player profile with its settings entry.")); return false; }
		return true;
	}
	if (View == EView::Settings)
	{
		if (Modal->IsModalOpen() || !Feature->IsScreenOpen() || !LanguageAuditVisible(Feature->GetWidgetFromName(TEXT("SettingsHubLanguageButton")))
			|| Feature->GetWidgetFromName(TEXT("LanguageConfirmButton")))
		{ Finish(TEXT("Expected the actual settings hub with its language entry.")); return false; }
		return true;
	}
	return ValidateLanguage();
}

bool URokLanguageSettingsAuditSubsystem::ValidateLanguage()
{
	const auto* Canvas = Feature->GetWidgetFromName(TEXT("FeatureModeCanvas"));
	const auto* Confirm = Cast<UButton>(Feature->GetWidgetFromName(TEXT("LanguageConfirmButton")));
	if (Modal->IsModalOpen() || !Feature->IsScreenOpen() || !Canvas || !LanguageAuditVisible(Confirm)
		|| Feature->GetLanguageDraftIdForAudit() != ExpectedDraftId || Confirm->GetIsEnabled() != (ExpectedDraftId == 40))
	{ Finish(TEXT("Language draft or same-language-only confirmation state mismatch.")); return false; }
	const FVector2D Scale = Canvas->GetRenderTransform().Scale; const float Opacity = Canvas->GetRenderOpacity();
	if (!FMath::IsNearlyEqual(Scale.X, Scale.Y, .0001)) { Finish(TEXT("T1 XY scales diverged.")); return false; }
	if (View == EView::Opening || View == EView::Closing)
	{
		const bool bOpening = View == EView::Opening;
		if (Canvas->GetVisibility() != ESlateVisibility::HitTestInvisible
			|| !(Scale.X > (bOpening ? 1.05 : 1.0) && Scale.X <= 1.1001)
			|| (bOpening ? !FMath::IsNearlyEqual(Opacity, 1.f) : !(Opacity > 0 && Opacity < 1)))
		{ Finish(TEXT("Missed the real T1 intermediate frame or its input shield; no synthetic advance is used.")); return false; }
		return true;
	}
	if (!Scale.Equals(FVector2D(1), .0001) || !FMath::IsNearlyEqual(Opacity, 1.f)
		|| Canvas->GetVisibility() != ESlateVisibility::SelfHitTestInvisible)
	{ Finish(TEXT("Language T1 animation did not settle at scale/opacity 1.")); return false; }
	const auto* Viewport = Feature->GetWidgetFromName(TEXT("LanguageSettingsViewport"));
	const auto* FrameImage = Cast<UImage>(Feature->GetWidgetFromName(TEXT("LanguageSettingsSourceFrame")));
	const auto* TitleRegion = Cast<UBorder>(Feature->GetWidgetFromName(TEXT("LanguageSettingsTitleRegion")));
	const auto* Title = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("FeatureFrameTitle")));
	const auto* Close = Cast<UButton>(Feature->GetWidgetFromName(TEXT("FeatureFrameClose")));
	const auto* ConfirmBackground = Feature->GetWidgetFromName(TEXT("LanguageConfirmButtonBackground"));
	const auto* ConfirmRegion = Cast<UBorder>(Feature->GetWidgetFromName(TEXT("LanguageConfirmLabelRegion")));
	const auto* ConfirmLabel = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("LanguageConfirmButtonLabel")));
	const auto* Gate = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("LanguageSettingsResourceGate")));
	if (!LanguageAuditVisible(Viewport) || !LanguageAuditGeometry(Viewport, FVector2D(278.5, 190.65), FVector2D(883, 370.4))
		|| Viewport->GetClipping() != EWidgetClipping::ClipToBoundsAlways
		|| !LanguageAuditVisible(Title) || Title->GetText().ToString() != TEXT("选择语言")
		|| !LanguageAuditVisible(ConfirmLabel) || ConfirmLabel->GetText().ToString() != TEXT("确定")
		|| !LanguageAuditGeometry(Confirm, FVector2D(583, 569.9), FVector2D(274, 84))
		|| !Gate || Gate->GetText().ToString() != TEXT("仅预览选择，暂不应用语言更改。")
		|| Gate->GetVisibility() != (ExpectedDraftId == 40 ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible))
	{ Finish(TEXT("Language source viewport, labels, confirmation geometry or draft boundary mismatch.")); return false; }
	// Native source probe native_20260912_094233_725 places TypeMid beside rect.
	// Only the list inherits rect's -25.9 offset; the complete source frame starts at Y123.
	if (!LanguageAuditVisible(FrameImage) || !LanguageAuditGeometry(FrameImage, FVector2D(271, 123), FVector2D(898, 564))
		|| FrameImage->GetParent() != Canvas || !FrameImage->GetRenderTransform().Scale.Equals(FVector2D(1), .001)
		|| !FrameImage->GetBrush().GetResourceObject()
		|| FrameImage->GetBrush().GetResourceObject()->GetPathName() != TEXT("/Game/RokPrototype/UI/ModalAtlases/rok_modal_atlas_frame.rok_modal_atlas_frame")
		|| Feature->GetWidgetFromName(TEXT("FeatureFrameHeaderLeft")) || Feature->GetWidgetFromName(TEXT("FeatureFrameHeaderRight"))
		|| Feature->GetWidgetFromName(TEXT("FeatureFrameBodyLeft")) || Feature->GetWidgetFromName(TEXT("FeatureFrameBodyRight")))
	{ Finish(TEXT("Language TypeMid frame is offset, missing its complete source art or contains generic frame pieces.")); return false; }
	const FBox2f FrameUV = FrameImage->GetBrush().GetUVRegion();
	if (!FrameUV.Min.Equals(FVector2f(1.f / 1024, 1.f / 2048), .00001f)
		|| !FrameUV.Max.Equals(FVector2f(899.f / 1024, 565.f / 2048), .00001f))
	{ Finish(TEXT("Language TypeMid does not use source pl_fra_1006 (1,1483,898,564).")); return false; }
	const auto* TitleJustification = FindFProperty<FByteProperty>(Title->GetClass(), TEXT("Justification"));
	const auto* ConfirmJustification = FindFProperty<FByteProperty>(ConfirmLabel->GetClass(), TEXT("Justification"));
	if (!LanguageAuditVisible(TitleRegion) || !LanguageAuditGeometry(TitleRegion, FVector2D(307.75, 133.5), FVector2D(824.5, 40))
		|| TitleRegion->GetParent() != Canvas || TitleRegion->GetVerticalAlignment() != VAlign_Center || Title->GetParent() != TitleRegion
		|| !FMath::IsNearlyEqual(Title->GetFont().Size, 22.5f) || Title->GetFont().TypefaceFontName != TEXT("Regular")
		|| !Title->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor::White, .0001f)
		|| !Title->GetShadowOffset().Equals(FVector2D::ZeroVector, .001)
		|| !TitleJustification || TitleJustification->GetPropertyValue_InContainer(Title) != uint8(ETextJustify::Center)
		|| !LanguageAuditVisible(Close) || !Close->GetIsEnabled() || Close->GetParent() != Canvas
		|| !LanguageAuditGeometry(Close, FVector2D(1095.5, 118.5), FVector2D(69)))
	{ Finish(TEXT("Language source title/close position, centered alignment, font or white title color mismatch.")); return false; }
	if (!LanguageAuditVisible(ConfirmRegion) || !LanguageAuditGeometry(ConfirmRegion, FVector2D(0, 10), FVector2D(274, 58))
		|| ConfirmRegion->GetVerticalAlignment() != VAlign_Center || ConfirmLabel->GetParent() != ConfirmRegion
		|| !FMath::IsNearlyEqual(ConfirmLabel->GetFont().Size, 24.f) || ConfirmLabel->GetFont().TypefaceFontName != TEXT("Bold")
		|| !ConfirmLabel->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor::FromSRGBColor(FColor(10, 27, 46, 255)), .00001f)
		|| ConfirmLabel->GetColorAndOpacity().GetSpecifiedColor().ToFColor(true) != FColor(10, 27, 46, 255)
		|| !ConfirmLabel->GetShadowOffset().Equals(FVector2D::ZeroVector, .001)
		|| !ConfirmJustification || ConfirmJustification->GetPropertyValue_InContainer(ConfirmLabel) != uint8(ETextJustify::Center)
		|| !LanguageAuditVisible(ConfirmBackground) || !ConfirmBackground->GetRenderTransform().Scale.Equals(FVector2D(-1, 1), .001)
		|| !ConfirmBackground->GetRenderTransformPivot().Equals(FVector2D(.5), .001))
	{ Finish(TEXT("Language confirmation source text region, dark-blue color, bold font or mirrored source button art mismatch.")); return false; }
	static const TCHAR* Labels[] = {TEXT("English"), TEXT("العربية"), TEXT("简体中文"), TEXT("Türkçe")};
	// Source bin order is ar/en/tr/zh-CN; Chinese Arab512 flips the grid to upper-left.
	static const FVector2D CellPositions[] = {FVector2D(451.5, -1.5), FVector2D(43.5, -1.5), FVector2D(451.5, 109.5), FVector2D(43.5, 109.5)};
	int32 SelectedCount = 0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const auto Named = [&](const TCHAR* Prefix) { return Feature->GetWidgetFromName(*FString::Printf(TEXT("%s_%d"), Prefix, Index)); };
		const auto* Button = Cast<UButton>(Feature->GetWidgetFromName(LanguageAuditButtons[Index]));
		const auto* Cell = Named(TEXT("LanguageOptionCell"));
		const auto* Background = Named(TEXT("LanguageOptionBackground"));
		const auto* Base = Cast<UImage>(Named(TEXT("LanguageOptionRadioBase")));
		const auto* Radio = Cast<UImage>(Named(TEXT("LanguageOptionRadio")));
		const auto* Region = Cast<UBorder>(Named(TEXT("LanguageOptionLabelRegion")));
		const auto* Label = Cast<UTextBlock>(Named(TEXT("LanguageOptionLabel")));
		const bool bSelected = LanguageAuditIds[Index] == ExpectedDraftId;
		if (!LanguageAuditVisible(Button) || !Button->GetIsEnabled() || !LanguageAuditVisible(Cell)
			|| Cell->GetParent() != Viewport || Button->GetParent() != Cell
			|| !LanguageAuditGeometry(Cell, CellPositions[Index], FVector2D(388, 114))
			|| !LanguageAuditGeometry(Button, FVector2D::ZeroVector, FVector2D(388, 114))
			|| !LanguageAuditVisible(Background) || !LanguageAuditGeometry(Background, FVector2D::ZeroVector, FVector2D(388, 114))
			|| !LanguageAuditVisible(Base) || !LanguageAuditAtlas(Base, 611, 181, 44, 44)
			|| !LanguageAuditGeometry(Base, FVector2D(30.3, 35), FVector2D(44))
			|| !LanguageAuditAtlas(Radio, 697, 231, 44, 44)
			|| !LanguageAuditGeometry(Radio, FVector2D(30.3, 35), FVector2D(44), bSelected)
			|| Radio->GetVisibility() != (bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed)
			|| (bSelected && !LanguageAuditVisible(Radio)))
		{ Finish(FString::Printf(TEXT("Source language option %d has wrong position, active radio or atlas."), LanguageAuditIds[Index])); return false; }
		SelectedCount += LanguageAuditVisible(Radio) ? 1 : 0;
		const auto* Justification = Label ? FindFProperty<FByteProperty>(Label->GetClass(), TEXT("Justification")) : nullptr;
		const auto* TextOptions = LanguageAuditTextOptions(Label);
		if (!LanguageAuditVisible(Region) || !LanguageAuditGeometry(Region, FVector2D(83.015, 0), FVector2D(282.37, 114))
			|| Region->GetVerticalAlignment() != VAlign_Center || !LanguageAuditVisible(Label) || Label->GetParent() != Region
			|| Label->GetText().ToString() != Labels[Index] || !FMath::IsNearlyEqual(Label->GetFont().Size, 19.5f)
			|| Label->GetFont().TypefaceFontName != TEXT("Regular") || !Justification
			|| Justification->GetPropertyValue_InContainer(Label) != uint8(ETextJustify::Left)
			|| Label->GetFlowDirectionPreference() != EFlowDirectionPreference::LeftToRight
			|| !Label->GetCachedWidget().IsValid() || Label->GetCachedWidget()->GetFlowDirectionPreference() != EFlowDirectionPreference::LeftToRight
			|| !TextOptions || !TextOptions->bOverride_TextFlowDirection || TextOptions->TextFlowDirection != ETextFlowDirection::LeftToRight
			|| TextOptions->bOverride_TextShapingMethod
			|| !Label->GetColorAndOpacity().GetSpecifiedColor().Equals(FLinearColor::White, .0001f)
			|| !Label->GetShadowOffset().Equals(FVector2D::ZeroVector, .001))
		{ Finish(FString::Printf(TEXT("Source language option %d text or Chinese alignment mismatch."), LanguageAuditIds[Index])); return false; }
		// Check every rendered nine-slice patch against pl_com_1000's source border.
		static const TCHAR* Suffixes[] = {TEXT("TopLeft"), TEXT("Top"), TEXT("TopRight"), TEXT("Left"), TEXT("Center"), TEXT("Right"), TEXT("BottomLeft"), TEXT("Bottom"), TEXT("BottomRight")};
		static const int32 X[] = {1952, 1975, 2005}, Y[] = {960, 928, 906}, W[] = {23, 30, 21}, H[] = {13, 32, 22};
		for (int32 Patch = 0; Patch < 9; ++Patch)
		{
			const auto* Image = Cast<UImage>(Feature->GetWidgetFromName(*FString::Printf(TEXT("LanguageOptionBackground_%d_%s"), Index, Suffixes[Patch])));
			if (!LanguageAuditVisible(Image) || !LanguageAuditAtlas(Image, X[Patch % 3], Y[Patch / 3], W[Patch % 3], H[Patch / 3]))
			{ Finish(TEXT("Source pl_com_1000 nine-slice art is missing or uses a different atlas region.")); return false; }
		}
	}
	if (SelectedCount != 1) { Finish(TEXT("Language radio group does not contain exactly one selected option.")); return false; }
	return true;
}
bool URokLanguageSettingsAuditSubsystem::Click(UUserWidget* Owner, FName Name)
{
	if (!Validate()) return false;
	bool bAllowed = Name == TEXT("PlayerProfileHitButton") || Name == TEXT("ProfileSettingsButton")
		|| Name == TEXT("SettingsHubLanguageButton") || Name == TEXT("FeatureFrameClose");
	for (const FName& Option : LanguageAuditButtons) bAllowed |= Name == Option;
	// The source confirmation for the unchanged current language only dismisses the window.
	bAllowed |= Name == TEXT("LanguageConfirmButton") && ExpectedDraftId == 40;
	const auto* Canvas = Owner == Feature.Get() ? Feature->GetWidgetFromName(TEXT("FeatureModeCanvas")) : nullptr;
	auto* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Name)) : nullptr;
	if (!bAllowed || !Button || Owner->GetWorld() != GetWorld() || !LanguageAuditVisible(Button) || !Button->GetIsEnabled()
		|| Button->GetCachedGeometry().GetLocalSize().X <= 0 || (Canvas && Canvas->GetVisibility() == ESlateVisibility::HitTestInvisible)
		|| (Owner == Modal.Get() && LanguageAuditVisible(Modal->GetWidgetFromName(TEXT("ModalAnimationInputBlocker")))))
	{ Finish(FString::Printf(TEXT("Actual permitted button is unavailable: %s."), *Name.ToString())); return false; }
	auto Action = MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button_delegate"), Name.ToString());
	Action->SetNumberField(TEXT("draft_before"), ExpectedDraftId); Action->SetNumberField(TEXT("frame"), double(Frame));
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	Button->OnClicked.Broadcast(); return true;
}
bool URokLanguageSettingsAuditSubsystem::Select(int32 SourceId)
{
	for (int32 Index = 0; Index < 4; ++Index) if (LanguageAuditIds[Index] == SourceId)
	{
		if (!Click(Feature.Get(), LanguageAuditButtons[Index])) return false;
		ExpectedDraftId = SourceId; Enter(Step + 1); return true;
	}
	Finish(TEXT("Audit attempted a language outside the enabled source config.")); return false;
}
void URokLanguageSettingsAuditSubsystem::Enter(int32 NextStep) { Step = NextStep; StepStarted = Elapsed; }
TSharedRef<FJsonObject> URokLanguageSettingsAuditSubsystem::State() const
{
	auto Result = MakeShared<FJsonObject>(); Result->SetNumberField(TEXT("step"), Step); Result->SetNumberField(TEXT("frame"), double(Frame));
	Result->SetNumberField(TEXT("time_seconds"), Elapsed); Result->SetNumberField(TEXT("expected_draft_id"), ExpectedDraftId);
	Result->SetNumberField(TEXT("culture_change_events"), CultureChangeCount);
	Result->SetNumberField(TEXT("disk_comparisons_passed"), DiskComparisonCount);
	if (Feature.IsValid())
	{
		Result->SetNumberField(TEXT("actual_draft_id"), Feature->GetLanguageDraftIdForAudit());
		if (const auto* Canvas = Feature->GetWidgetFromName(TEXT("FeatureModeCanvas")))
		{
			Result->SetNumberField(TEXT("actual_window_scale"), Canvas->GetRenderTransform().Scale.X);
			Result->SetNumberField(TEXT("actual_window_opacity"), Canvas->GetRenderOpacity());
		}
		if (const auto* Confirm = Cast<UButton>(Feature->GetWidgetFromName(TEXT("LanguageConfirmButton"))))
			Result->SetBoolField(TEXT("confirm_enabled"), Confirm->GetIsEnabled());
		TArray<TSharedPtr<FJsonValue>> Options;
		for (int32 Index = 0; Index < 4; ++Index)
			if (const auto* Radio = Feature->GetWidgetFromName(*FString::Printf(TEXT("LanguageOptionRadio_%d"), Index)))
			{
				auto Item = MakeShared<FJsonObject>(); Item->SetNumberField(TEXT("source_id"), LanguageAuditIds[Index]);
				Item->SetBoolField(TEXT("radio_visible"), LanguageAuditVisible(Radio)); Options.Add(MakeShared<FJsonValueObject>(Item));
			}
		Result->SetArrayField(TEXT("actual_radios"), Options);
		TArray<TSharedPtr<FJsonValue>> Geometry;
		for (const TCHAR* Name : {TEXT("LanguageSettingsSourceFrame"), TEXT("LanguageSettingsTitleRegion"), TEXT("FeatureFrameClose"),
			TEXT("LanguageConfirmButton"), TEXT("LanguageConfirmLabelRegion")})
			if (const auto* Widget = Feature->GetWidgetFromName(Name))
				if (const auto* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
				{
					auto Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("widget"), Name);
					Item->SetNumberField(TEXT("local_x"), Slot->GetPosition().X); Item->SetNumberField(TEXT("local_y"), Slot->GetPosition().Y);
					Item->SetNumberField(TEXT("width"), Slot->GetSize().X); Item->SetNumberField(TEXT("height"), Slot->GetSize().Y);
					Geometry.Add(MakeShared<FJsonValueObject>(Item));
				}
		Result->SetArrayField(TEXT("actual_source_frame_geometry"), Geometry);
		TArray<TSharedPtr<FJsonValue>> TextStyles;
		for (const TCHAR* Name : {TEXT("FeatureFrameTitle"), TEXT("LanguageConfirmButtonLabel"), TEXT("LanguageOptionLabel_1")})
			if (const auto* Text = Cast<UTextBlock>(Feature->GetWidgetFromName(Name)))
			{
				auto Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("widget"), Name);
				Item->SetNumberField(TEXT("font_points"), Text->GetFont().Size); Item->SetStringField(TEXT("typeface"), Text->GetFont().TypefaceFontName.ToString());
				const FLinearColor Color = Text->GetColorAndOpacity().GetSpecifiedColor();
				Item->SetNumberField(TEXT("red"), Color.R); Item->SetNumberField(TEXT("green"), Color.G); Item->SetNumberField(TEXT("blue"), Color.B); Item->SetNumberField(TEXT("alpha"), Color.A);
				const FColor SRGB = Color.ToFColor(true);
				Item->SetNumberField(TEXT("srgb_red8"), SRGB.R); Item->SetNumberField(TEXT("srgb_green8"), SRGB.G); Item->SetNumberField(TEXT("srgb_blue8"), SRGB.B);
				Item->SetBoolField(TEXT("umg_left_to_right"), Text->GetFlowDirectionPreference() == EFlowDirectionPreference::LeftToRight);
				Item->SetBoolField(TEXT("slate_left_to_right"), Text->GetCachedWidget().IsValid() && Text->GetCachedWidget()->GetFlowDirectionPreference() == EFlowDirectionPreference::LeftToRight);
				if (const auto* TextOptions = LanguageAuditTextOptions(Text))
				{
					Item->SetBoolField(TEXT("shaped_text_flow_override"), TextOptions->bOverride_TextFlowDirection);
					Item->SetBoolField(TEXT("shaped_text_flow_left_to_right"), TextOptions->TextFlowDirection == ETextFlowDirection::LeftToRight);
					Item->SetBoolField(TEXT("shaping_method_override"), TextOptions->bOverride_TextShapingMethod);
				}
				TextStyles.Add(MakeShared<FJsonValueObject>(Item));
			}
		Result->SetArrayField(TEXT("actual_source_text_styles"), TextStyles);
	}
	return Result;
}
void URokLanguageSettingsAuditSubsystem::Capture(const TCHAR* Name)
{
	if (!Validate()) return;
	if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Another screenshot is already pending.")); return; }
	PendingPath = Directory / (FString(Name) + TEXT(".png"));
	if (IFileManager::Get().FileExists(*PendingPath)) { Finish(TEXT("Screenshot path already exists; refusing stale evidence.")); return; }
	PendingCapture = MakeShared<FJsonObject>(); PendingCapture->SetStringField(TEXT("name"), Name); PendingCapture->SetStringField(TEXT("path"), PendingPath);
	PendingCapture->SetObjectField(TEXT("state_at_request"), State()); ScreenshotStarted = Elapsed;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}
void URokLanguageSettingsAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !PendingCapture.IsValid() || !Validate()) return;
	TArray<uint8> Bytes; const uint8 Signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
	if (!FFileHelper::LoadFileToArray(Bytes, *PendingPath) || Bytes.Num() < 24 || FMemory::Memcmp(Bytes.GetData(), Signature, 8) != 0)
	{ Finish(TEXT("Processed screenshot is absent, empty or not PNG.")); return; }
	const auto ReadBE = [&Bytes](int32 Offset) { return (uint32(Bytes[Offset]) << 24) | (uint32(Bytes[Offset + 1]) << 16) | (uint32(Bytes[Offset + 2]) << 8) | uint32(Bytes[Offset + 3]); };
	if (ReadBE(16) < 2 || ReadBE(20) < 2) { Finish(TEXT("Invalid screenshot dimensions.")); return; }
	PendingCapture->SetNumberField(TEXT("file_bytes"), Bytes.Num()); PendingCapture->SetNumberField(TEXT("width"), ReadBE(16)); PendingCapture->SetNumberField(TEXT("height"), ReadBE(20));
	PendingCapture->SetObjectField(TEXT("state_at_processed"), State()); Captures.Add(MakeShared<FJsonValueObject>(PendingCapture)); PendingCapture.Reset();
	if (!Save(false)) { Finish(TEXT("Progress manifest save failed.")); return; }
	Enter(Step + 1);
}
void URokLanguageSettingsAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !GetWorld()->HasBegunPlay()) return;
	Elapsed += FMath::Max(DeltaTime, 0.f); ++Frame;
	if (Elapsed > 90) { Finish(TEXT("Language audit timed out.")); return; }
	if (bSnapshot && !CheckUnchanged(false)) return;
	if (PendingCapture.IsValid()) { if (Elapsed - ScreenshotStarted > 10) Finish(TEXT("Screenshot callback timed out.")); return; }
	const double Delay = Step == 0 ? 3.0 : (Step == 6 || Step == 21 || Step == 30 ? .05 : .5);
	if (Elapsed - StepStarted < Delay) return;
	switch (Step)
	{
	case 0: if (Begin()) Capture(TEXT("01_city_before")); break;
	case 1: if (Click(Hud.Get(), TEXT("PlayerProfileHitButton"))) { View = EView::Profile; Enter(2); } break;
	case 2: Capture(TEXT("02_profile")); break;
	case 3: if (Click(Modal.Get(), TEXT("ProfileSettingsButton"))) { View = EView::Settings; Enter(4); } break;
	case 4: Capture(TEXT("03_settings_hub")); break;
	case 5: if (Click(Feature.Get(), TEXT("SettingsHubLanguageButton"))) { View = EView::Opening; Enter(6); } break;
	case 6: Capture(TEXT("04_t1_opening")); break;
	case 7: View = EView::Language; Capture(TEXT("05_initial_chinese_40")); break;
	case 8: Select(1); break;
	case 9: Capture(TEXT("06_arabic_1")); break;
	case 10: Select(1); break;
	case 11: Capture(TEXT("07_arabic_reselected")); break;
	case 12: Select(10); break;
	case 13: Capture(TEXT("08_english_10")); break;
	case 14: Select(37); break;
	case 15: Capture(TEXT("09_turkish_37")); break;
	case 16: Select(40); break;
	case 17: Capture(TEXT("10_chinese_40")); break;
	case 18: Select(10); break;
	case 19: Capture(TEXT("11_english_before_cancel")); break;
	case 20: if (Click(Feature.Get(), TEXT("FeatureFrameClose"))) { bObservedLanguageRemoval = false; View = EView::Closing; Enter(21); } break;
	case 21: Capture(TEXT("12_t1_cancel_closing")); break;
	case 22: View = EView::City; Capture(TEXT("13_cancelled_to_city")); break;
	case 23: if (Click(Hud.Get(), TEXT("PlayerProfileHitButton"))) { View = EView::Profile; Enter(24); } break;
	case 24: if (Click(Modal.Get(), TEXT("ProfileSettingsButton"))) { View = EView::Settings; Enter(25); } break;
	case 25:
		if (!bObservedLanguageRemoval) { Finish(TEXT("Language controls were not observed removed before reopening.")); break; }
		if (Click(Feature.Get(), TEXT("SettingsHubLanguageButton"))) { ExpectedDraftId = 40; View = EView::Language; Enter(26); } break;
	case 26: Capture(TEXT("14_reopen_draft_reset_40")); break;
	case 27: Select(40); break;
	case 28: Capture(TEXT("15_chinese_reselected")); break;
	case 29: if (Click(Feature.Get(), TEXT("LanguageConfirmButton"))) { View = EView::Closing; Enter(30); } break;
	case 30: Capture(TEXT("16_t1_unchanged_confirm_closing")); break;
	case 31: View = EView::City; Capture(TEXT("17_city_after")); break;
	case 32: if (Validate()) Finish(); break;
	default: Finish(TEXT("Unknown language-audit stage.")); break;
	}
}
bool URokLanguageSettingsAuditSubsystem::Save(bool bComplete)
{
	auto Root = MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("complete"), bComplete);
	Root->SetBoolField(TEXT("passed"), bComplete && Failures.IsEmpty() && Captures.Num() == LanguageAuditCaptureCount);
	Root->SetNumberField(TEXT("audit_version"), 3); Root->SetNumberField(TEXT("expected_capture_count"), LanguageAuditCaptureCount);
	Root->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Root->SetStringField(TEXT("world"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : TEXT("unavailable"));
	Root->SetStringField(TEXT("input_method"), TEXT("Actual UMG OnClicked delegates and normal widget ticks. No physical pointer/hit-test or synthetic animation advance claim."));
	Root->SetStringField(TEXT("scope"), TEXT("Source-derived local language draft only. Selecting a different language never invokes confirmation. Confirmation is exercised only for the unchanged Chinese presentation baseline and must dismiss without applying/saving/reloading. No account/server/localization-package completion claim."));
	Root->SetStringField(TEXT("source"), TEXT("UI_Win_Language/UI_LC_Language/UI_Item_LanguageSelect; LanguageSetView creates a mediator; GameMediator.OnRegister calls InitData; LanguageSetMediator.InitData resets to active language and OnValueChange only changes a draft. UI_Win_Language uses shared T1 On/Off."));
	Root->SetBoolField(TEXT("unity_visual_comparison_complete"), false);
	Root->SetStringField(TEXT("text_flow_observation"), TEXT("UMG ShapedTextOptions and both widget flow preferences are checked. UE 5.8 STextBlock has no public GetTextFlowDirection; glyph ink placement requires separate PNG verification."));
	Root->SetStringField(TEXT("native_layout_reference"), TEXT("Saved/UnityLanguageNativeReferenceProbe/Artifacts/native_20260912_094233_725/render_evidence.json"));
	Root->SetStringField(TEXT("baseline_engine_language"), InitialLanguage); Root->SetStringField(TEXT("baseline_engine_locale"), InitialLocale);
	Root->SetNumberField(TEXT("culture_change_events"), CultureChangeCount);
	Root->SetNumberField(TEXT("disk_comparisons_passed"), DiskComparisonCount);
	Root->SetBoolField(TEXT("preference_baseline_established"), bSnapshot);
	auto Branches = MakeShared<FJsonObject>();
	for (const auto& Pair : PreferencePathsByKey) Branches->SetStringField(Pair.Key, Pair.Value);
	Root->SetObjectField(TEXT("known_config_key_to_actual_ini"), Branches);
	Root->SetObjectField(TEXT("last_observed_state"), State());
	TArray<TSharedPtr<FJsonValue>> Files;
	for (const auto& File : PreferenceFiles)
	{
		auto Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("path"), File.Path);
		Item->SetBoolField(TEXT("initial_exists"), File.bExists); Item->SetNumberField(TEXT("initial_bytes"), File.Bytes.Num());
		Files.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("preference_files_compared_by_content"), Files); Root->SetArrayField(TEXT("actions"), Actions); Root->SetArrayField(TEXT("captures"), Captures);
	TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error)); Root->SetArrayField(TEXT("failures"), Errors);
	FString Json; if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json))) return false;
	return FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokLanguageSettingsAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	if (Failures.IsEmpty() && Captures.Num() != LanguageAuditCaptureCount) Failures.Add(TEXT("Expected exactly 17 processed screenshots."));
	if (!Save(Failures.IsEmpty())) Failures.Add(TEXT("Final manifest save failed."));
	bFinished = true;
	UE_LOG(LogTemp, Display, TEXT("Rok LanguageSettings audit passed=%d captures=%d failures=%d output=%s"), Failures.IsEmpty(), Captures.Num(), Failures.Num(), *Directory);
	for (const auto& Error : Failures) UE_LOG(LogTemp, Error, TEXT("Rok LanguageSettings audit: %s"), *Error);
	FPlatformMisc::RequestExit(false);
}
