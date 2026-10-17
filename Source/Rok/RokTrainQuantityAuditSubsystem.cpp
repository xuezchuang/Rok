#include "RokTrainQuantityAuditSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableText.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokMainHudWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityGameplayCatalogSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
bool QuantityVisible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible()) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible()) return false;
	for (const UObject* Outer = Widget->GetOuter(); Outer; Outer = Outer->GetOuter())
		if (const auto* Parent = Cast<UUserWidget>(Outer); Parent && !Parent->IsVisible()) return false;
	return true;
}
}

bool URokTrainQuantityAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokTrainQuantityAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokTrainQuantityAuditSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const { return WorldType == EWorldType::Game; }
void URokTrainQuantityAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	OutputDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/TrainQuantityAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*OutputDirectory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokTrainQuantityAuditSubsystem::ScreenshotProcessed);
}
void URokTrainQuantityAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	Super::Deinitialize();
}
TStatId URokTrainQuantityAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokTrainQuantityAuditSubsystem, STATGROUP_Tickables); }

bool URokTrainQuantityAuditSubsystem::FindRuntime()
{
	for (const TCHAR* Other : {TEXT("RokTrainTierAudit"), TEXT("RokUiAnimationAudit"), TEXT("RokWorldEntityAnimationAudit"), TEXT("RokPlayerHeadAudit")})
		if (FParse::Param(FCommandLine::Get(), Other)) { Fail(TEXT("Quantity audit requires a dedicated runtime process.")); return false; }
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FPaths::NormalizeDirectoryName(Project);
	if (!Project.Equals(TEXT("D:/ueDemo/Rok"), ESearchCase::IgnoreCase) || GetWorld()->GetName() != TEXT("L_RokUnityGameAppCity"))
	{ Fail(TEXT("Wrong project/world; no UI action performed.")); return false; }
	FString AutoSelect;
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokAutoSelectBuilding="), AutoSelect) || AutoSelect != TEXT("UnityInitId_6"))
	{ Fail(TEXT("Requires -RokAutoSelectBuilding=UnityInitId_6.")); return false; }
	int32 Count = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It) if (It->ActorHasTag(TEXT("UnityInitId_6"))) ++Count;
	if (Count != 1) { Fail(TEXT("Expected one actual source barracks.")); return false; }
	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, URokMainHudWidget::StaticClass(), false);
	Count = 0;
	for (auto* Widget : Found) if (Widget->GetWorld() == GetWorld() && QuantityVisible(Widget) && Widget->GetCachedGeometry().GetLocalSize().X > 1)
	{ Hud = Cast<URokMainHudWidget>(Widget); ++Count; }
	if (Count != 1) { Fail(TEXT("Expected one attached city HUD.")); return false; }
	Found.Reset(); Count = 0;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, URokUnityFeatureScreenWidget::StaticClass(), false);
	for (auto* Widget : Found) if (Widget->GetWorld() == GetWorld()) { Feature = Cast<URokUnityFeatureScreenWidget>(Widget); ++Count; }
	if (Count != 1 || Feature->IsScreenOpen()) { Fail(TEXT("Expected one closed feature host.")); return false; }
	auto* Catalog = GetWorld()->GetGameInstance()->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	if (!Catalog) { Fail(TEXT("Missing source catalog.")); return false; }
	const auto Rows = Catalog->GetTroopsForArmyType(1);
	const auto* Row = Rows.FindByPredicate([](const auto& R) { return R.Id == 10101; });
	if (!Row || Row->TrainCost.Food != 50 || Row->TrainCost.Wood != 50 || Row->TrainCost.Stone != 0 || Row->TrainCost.Gold != 0 || Row->TrainSeconds != 15)
	{ Fail(TEXT("Source Arms 10101 cost/time changed; audit expectations need source review.")); return false; }
	return true;
}

bool URokTrainQuantityAuditSubsystem::BindControls()
{
	Input = Cast<UEditableTextBox>(Feature->GetWidgetFromName(TEXT("TrainArmyQuantityInput")));
	Slider = Cast<USlider>(Feature->GetWidgetFromName(TEXT("TrainArmyQuantitySlider")));
	if (!QuantityVisible(Input.Get()) || !QuantityVisible(Slider.Get()) || !Input->GetIsEnabled() || !Slider->GetIsEnabled())
	{ Fail(TEXT("Missing active quantity controls.")); return false; }
	return true;
}
bool URokTrainQuantityAuditSubsystem::Click(UUserWidget* Owner, FName Name)
{
	if (Name == TEXT("TrainArmyStart")) { Fail(TEXT("Training command forbidden.")); return false; }
	auto* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Name)) : nullptr;
	if (!Button || Owner->GetWorld() != GetWorld() || !QuantityVisible(Button) || !Button->GetIsEnabled() || Button->GetCachedGeometry().GetLocalSize().X <= 0)
	{ Fail(FString::Printf(TEXT("Missing/hidden/disabled button %s"), *Name.ToString())); return false; }
	auto Action = MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button_delegate"), Name.ToString());
	Action->SetNumberField(TEXT("frame"), double(Frame)); Actions.Add(MakeShared<FJsonValueObject>(Action));
	Button->OnClicked.Broadcast(); return true;
}
bool URokTrainQuantityAuditSubsystem::Validate()
{
#if !UE_BUILD_SHIPPING
	if (!Feature.IsValid() || !Feature->IsScreenOpen() || Feature->GetSelectedTrainArmyTypeForAudit() != 1
		|| Feature->GetSelectedTrainTierForAudit() != (bUnknownTier ? 2 : 1))
	{ Fail(TEXT("Unexpected training screen/tier.")); return false; }
	const AActor* Context = Feature->GetFeatureContextActorForAudit();
	if (!Context || Context->GetWorld() != GetWorld() || !Context->ActorHasTag(TEXT("UnityInitId_6")))
	{ Fail(TEXT("Lost source barracks context.")); return false; }
	if (bUnknownTier)
	{
		for (const TCHAR* Name : {TEXT("TrainArmyStart"), TEXT("TrainArmyPlus"), TEXT("TrainArmyMinus"), TEXT("TrainArmyMax"), TEXT("TrainArmyQuantity"), TEXT("TrainArmyCostAndTime"), TEXT("TrainArmyQuantityInput"), TEXT("TrainArmyQuantitySlider"), TEXT("TrainArmyQuantityShade"), TEXT("TrainArmyQuantitySliderBackground"), TEXT("TrainArmyQuantitySliderFill")})
			if (QuantityVisible(Feature->GetWidgetFromName(Name))) { Fail(FString::Printf(TEXT("Unknown T2 exposes %s"), Name)); return false; }
		if (QuantityVisible(Feature->GetWidgetFromName(TEXT("TrainResourceCostRoot"))))
		{ Fail(TEXT("Unknown T2 exposes training resource costs.")); return false; }
		auto* Status = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("TrainArmyStudyStatus")));
		if (!QuantityVisible(Status) || Status->GetText().ToString() != TEXT("当前兵阶需要科技解锁，解锁状态待同步。可查看兵种详情。"))
		{ Fail(TEXT("Unknown technology status missing.")); return false; }
		return true;
	}
	if (!Input.IsValid() || !Slider.IsValid() || Feature->GetWidgetFromName(TEXT("TrainArmyQuantityInput")) != Input.Get()
		|| Feature->GetWidgetFromName(TEXT("TrainArmyQuantitySlider")) != Slider.Get())
	{ Fail(TEXT("Quantity edit rebuilt input/slider tree.")); return false; }
	if (!QuantityVisible(Input.Get()) || !QuantityVisible(Slider.Get()) || Feature->GetSelectedTrainCountForAudit() != ExpectedCount
		|| Input->GetText().ToString() != FString::FromInt(ExpectedCount) || !FMath::IsNearlyEqual(Slider->GetValue(), ExpectedSlider, 0.0001f))
	{ Fail(FString::Printf(TEXT("Quantity sync mismatch expected %d/%.4f actual count %d/slider %.4f; input text omitted"), ExpectedCount, ExpectedSlider,
		Feature->GetSelectedTrainCountForAudit(), Slider->GetValue())); return false; }
	if (bCheckFocus && (!FocusedWidget.IsValid() || FSlateApplication::Get().GetUserFocusedWidget(0) != FocusedWidget.Pin()
		|| !(Input->HasKeyboardFocus() || Input->HasUserFocus(GetWorld()->GetFirstPlayerController()) || Input->HasUserFocusedDescendants(GetWorld()->GetFirstPlayerController()))))
	{ Fail(TEXT("Editing quantity lost actual Slate keyboard focus.")); return false; }
	const int32 TotalSeconds = ExpectedCount * 15;
	const FString TimeLine = FString::Printf(TEXT("基础训练时间：00:%02d:%02d  ·  加成、库存待同步"), TotalSeconds / 60, TotalSeconds % 60);
	const FString Amount = ExpectedCount == 20 ? TEXT("1.0K") : FString::FromInt(ExpectedCount * 50);
	auto* Time = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("TrainResourceBaseTime")));
	auto* Label = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("TrainArmyQuantity")));
	if (!QuantityVisible(Time) || Time->GetText().ToString() != TimeLine
		|| !QuantityVisible(Label) || Label->GetText().ToString() != FString::FromInt(ExpectedCount))
	{ Fail(TEXT("Quantity label or source-derived cost/time failed to update.")); return false; }
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Cell = Feature->GetWidgetFromName(*FString::Printf(TEXT("TrainResourceCostCell_%d"), Index));
		auto* Text = Cast<UTextBlock>(Feature->GetWidgetFromName(*FString::Printf(TEXT("TrainResourceCostAmount_%d"), Index)));
		auto* Icon = Feature->GetWidgetFromName(*FString::Printf(TEXT("TrainResourceCostIcon_%d"), Index));
		if (Index < 2 ? (!QuantityVisible(Cell) || !QuantityVisible(Text) || !QuantityVisible(Icon) || Text->GetText().ToString() != Amount)
			: (QuantityVisible(Cell) || QuantityVisible(Text) || QuantityVisible(Icon)))
		{ Fail(TEXT("Source resource row failed amount update or zero-cost visibility.")); return false; }
	}
#endif
	return true;
}
void URokTrainQuantityAuditSubsystem::EditText(const FString& Value, int32 Count)
{
	ExpectedCount = Count; ExpectedSlider = Count / 20.0f;
	const auto Focus = FocusedWidget.Pin();
	if (!Focus.IsValid() || Focus->GetType() != FName(TEXT("SEditableText"))
		|| FSlateApplication::Get().GetUserFocusedWidget(0) != Focus)
	{ Fail(TEXT("Expected actual focused SEditableText for whole-string edit.")); return; }
	// Public Slate SetText leaves UMG's Text untouched until OnBoundTextChanged
	// broadcasts through the actual UMG binding. UMG SetText preassigns Text and
	// suppresses that notification. This whole-string fixture is not keyboard input.
	StaticCastSharedPtr<SEditableText>(Focus)->SetText(FText::FromString(Value));
	auto Action = MakeShared<FJsonObject>(); Action->SetStringField(TEXT("slate_set_text_fixture"), Value);
	Actions.Add(MakeShared<FJsonValueObject>(Action)); Advance();
}
bool URokTrainQuantityAuditSubsystem::KeyboardReplace(bool bEmpty)
{
	if (!FocusedWidget.IsValid() || FSlateApplication::Get().GetUserFocusedWidget(0) != FocusedWidget.Pin())
	{ Fail(TEXT("Keyboard fixture lost input focus.")); return false; }
	auto& Slate = FSlateApplication::Get();
	const FModifierKeysState Control(false, false, true, false, false, false, false, false, false);
	const FModifierKeysState Plain;
	const bool bSelected = Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::A, Control, uint32(0), false, 0, 0));
	Slate.ProcessKeyUpEvent(FKeyEvent(EKeys::A, Control, uint32(0), false, 0, 0));
	bool bEdited;
	if (bEmpty)
	{
		bEdited = Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::BackSpace, Plain, uint32(0), false, 0, 0));
		Slate.ProcessKeyUpEvent(FKeyEvent(EKeys::BackSpace, Plain, uint32(0), false, 0, 0));
	}
	else bEdited = Slate.ProcessKeyCharEvent(FCharacterEvent(TEXT('7'), Plain, uint32(0), false));
	if (!bSelected || !bEdited) { Fail(TEXT("Slate replacement key event not handled.")); return false; }
	ExpectedCount = bEmpty ? 1 : 7; ExpectedSlider = ExpectedCount / 20.0f;
	auto Action = MakeShared<FJsonObject>(); Action->SetStringField(TEXT("slate_key_fixture"), bEmpty ? TEXT("select_all_backspace") : TEXT("select_all_character_7"));
	Actions.Add(MakeShared<FJsonValueObject>(Action)); Advance(); return true;
}
void URokTrainQuantityAuditSubsystem::EditSlider(float Value, int32 Count)
{
	ExpectedCount = Count; ExpectedSlider = Value;
	auto Action = MakeShared<FJsonObject>(); Action->SetNumberField(TEXT("slider_delegate"), Value); Actions.Add(MakeShared<FJsonValueObject>(Action));
	// SetValue alone does not simulate the UMG OnValueChanged event. This is not physical dragging.
	Slider->SetValue(Value); Slider->OnValueChanged.Broadcast(Value); Advance();
}
void URokTrainQuantityAuditSubsystem::Advance() { ++Step; StepStarted = Elapsed; }
void URokTrainQuantityAuditSubsystem::Capture(const FString& Name)
{
	if (!Validate()) return;
	if (FScreenshotRequest::IsScreenshotRequested()) { Fail(TEXT("Another screenshot request is active.")); return; }
	ScreenshotPath = OutputDirectory / (Name + TEXT(".png"));
	PendingCapture = MakeShared<FJsonObject>();
	PendingCapture->SetStringField(TEXT("name"), Name); PendingCapture->SetStringField(TEXT("path"), ScreenshotPath);
	PendingCapture->SetNumberField(TEXT("expected_count"), ExpectedCount); PendingCapture->SetNumberField(TEXT("expected_slider"), ExpectedSlider);
	PendingCapture->SetBoolField(TEXT("unknown_tier"), bUnknownTier); PendingCapture->SetBoolField(TEXT("focus_checked"), bCheckFocus);
	PendingCapture->SetNumberField(TEXT("request_frame"), double(Frame)); RequestedAt = Elapsed;
	FScreenshotRequest::RequestScreenshot(ScreenshotPath, true, false, false, FIntRect(), true);
	Advance();
}
void URokTrainQuantityAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !PendingCapture.IsValid() || !Validate()) return;
	const int64 Bytes = IFileManager::Get().FileSize(*ScreenshotPath);
	if (Bytes <= 0) { Fail(TEXT("Processed screenshot is absent/empty.")); return; }
	PendingCapture->SetNumberField(TEXT("bytes"), double(Bytes)); PendingCapture->SetNumberField(TEXT("processed_frame"), double(Frame));
	Captures.Add(MakeShared<FJsonValueObject>(PendingCapture)); PendingCapture.Reset();
	if (!Save(false)) Fail(TEXT("Unable to save progress manifest."));
}
void URokTrainQuantityAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished) return;
	Elapsed += FMath::Max(DeltaTime, 0.0f); ++Frame;
	if (Elapsed > 120) { Fail(TEXT("Quantity audit timed out.")); return; }
	if (PendingCapture.IsValid()) { if (Elapsed - RequestedAt > 10) Fail(TEXT("Screenshot timed out.")); return; }
	if (Elapsed - StepStarted < (Step == 0 ? 3.0 : 0.5)) return;
#if !UE_BUILD_SHIPPING
	switch (Step)
	{
	case 0: if (FindRuntime() && Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))), TEXT("BuildingFeatureButton"))) Advance(); break;
	case 1: if (BindControls() && Validate()) { Input->SetKeyboardFocus(); Advance(); } break;
	case 2: FocusedWidget = FSlateApplication::Get().GetUserFocusedWidget(0); bCheckFocus = true; if (Validate()) KeyboardReplace(false); break;
	case 3: if (Validate()) KeyboardReplace(true); break;
	case 4: if (Validate()) EditText(TEXT("0007"), 7); break;
	case 5: Capture(TEXT("01_text_leading_zero_7")); break;
	case 6: EditText(TEXT("abc"), 1); break;
	case 7: Capture(TEXT("02_text_invalid_1")); break;
	case 8: KeyboardReplace(false); break;
	case 9: if (Validate()) EditText(TEXT("2147483648"), 1); break;
	case 10: Capture(TEXT("03_text_overflow_1")); break;
	case 11: EditText(TEXT("999"), 20); break;
	case 12: Capture(TEXT("04_text_capacity_20")); break;
	case 13: EditSlider(0.5f, 10); break;
	case 14: Capture(TEXT("05_slider_half_10")); break;
	case 15: EditSlider(0.0f, 1); break;
	case 16: Capture(TEXT("06_slider_zero_1")); break;
	case 17: EditSlider(1.0f, 20); break;
	case 18: Capture(TEXT("07_slider_full_20")); break;
	case 19: bCheckFocus = false; if (Click(Feature.Get(), TEXT("TrainArmyTier_1"))) { bUnknownTier = true; Advance(); } break;
	case 20: Capture(TEXT("08_tier2_unknown_hidden")); break;
	case 21: if (Click(Feature.Get(), TEXT("TrainArmyTier_0"))) { bUnknownTier = false; ExpectedCount = 20; ExpectedSlider = 1; Advance(); } break;
	case 22: if (BindControls() && Validate() && Click(Feature.Get(), TEXT("FeatureFrameClose"))) Advance(); break;
	case 23:
		if (Feature->IsScreenOpen()) { Fail(TEXT("Window failed to close.")); return; }
		if (Click(Cast<UUserWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget"))), TEXT("BuildingFeatureButton"))) { ExpectedCount = 1; ExpectedSlider = 0.05f; Advance(); } break;
	case 24: if (BindControls()) Capture(TEXT("09_reopen_reset_1")); break;
	case 25: if (Click(Feature.Get(), TEXT("FeatureFrameClose"))) Advance(); break;
	case 26: if (Feature->IsScreenOpen()) Fail(TEXT("Reopened window failed to close.")); else Finish(); break;
	}
#endif
}
bool URokTrainQuantityAuditSubsystem::Save(bool bComplete)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("complete"), bComplete); Root->SetBoolField(TEXT("passed"), bComplete && Failures.IsEmpty() && Captures.Num() == 9);
	Root->SetStringField(TEXT("world"), GetWorld()->GetName()); Root->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Root->SetStringField(TEXT("source_expectations"), TEXT("Initial barracks UnityInitId_6 level1 capacity20; Arms10101 Food50 Wood50 Stone0 Gold0 Seconds15. Runtime resource cells update source CurrencyFormat amounts; zero-cost cells are hidden; base time remains exact without fabricated bonuses."));
	Root->SetStringField(TEXT("input_method"), TEXT("Focused Slate Ctrl+A/character7 and Ctrl+A/Backspace verify key routing. Whole-string boundary fixtures use public SEditableText::SetText and actual bound OnTextChanged, not physical typing. No clipboard access. Slider/buttons use UMG delegates; real runtime ticks; no pointer hit-test claim."));
	Root->SetStringField(TEXT("authority_boundary"), TEXT("No TrainArmyStart, training dispatch, inventory changes, private handlers, or TestingAdvance. Technology/server inventory remain unknown."));
	Root->SetArrayField(TEXT("actions"), Actions); Root->SetArrayField(TEXT("captures"), Captures);
	TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Root->SetArrayField(TEXT("failures"), Errors);
	FString Json; if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json))) return false;
	return FFileHelper::SaveStringToFile(Json, *(OutputDirectory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokTrainQuantityAuditSubsystem::Fail(const FString& Reason)
{
	if (bFinished) return;
	Failures.Add(Reason); bFinished = true; Save(false);
	UE_LOG(LogTemp, Error, TEXT("Rok TrainQuantity audit FAIL: %s; %s"), *Reason, *OutputDirectory);
	FPlatformMisc::RequestExit(false);
}
void URokTrainQuantityAuditSubsystem::Finish()
{
	if (Captures.Num() != 9) { Fail(TEXT("Expected exactly nine processed screenshots.")); return; }
	if (!Save(true)) { Fail(TEXT("Final manifest save failed.")); return; }
	bFinished = true; UE_LOG(LogTemp, Display, TEXT("Rok TrainQuantity audit PASS: %s"), *OutputDirectory);
	FPlatformMisc::RequestExit(false);
}


