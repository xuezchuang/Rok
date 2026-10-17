#include "RokChangeNameAuditSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokMainHudWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SEditableText.h"

namespace
{
constexpr int32 ChangeNameAuditCaptureCount = 15;
const TCHAR* EmptyFeedback = TEXT("请输入新的城主名称。");
const TCHAR* ValidFeedback = TEXT("本地格式校验通过；服务器仍需复核重名、禁词和消耗。");
bool ChangeNameAuditVisible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
	for (const UObject* Outer = Widget->GetOuter(); Outer; Outer = Outer->GetOuter())
		if (const auto* Parent = Cast<UUserWidget>(Outer); Parent && (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0)) return false;
	return true;
}
bool ChangeNameAuditRect(const UWidget* Widget, FVector2D Position, FVector2D Size)
{
	const auto* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
	return Slot && Slot->GetPosition().Equals(Position, .02) && Slot->GetSize().Equals(Size, .02)
		&& Widget->GetCachedGeometry().GetLocalSize().Equals(Size, .1);
}
}

bool URokChangeNameAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokChangeNameAudit")) && !IsRunningDedicatedServer()
		&& Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokChangeNameAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokChangeNameAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/ChangeNameAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokChangeNameAuditSubsystem::ScreenshotProcessed);
}
void URokChangeNameAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	Super::Deinitialize();
}
TStatId URokChangeNameAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokChangeNameAuditSubsystem, STATGROUP_Tickables); }

bool URokChangeNameAuditSubsystem::CheckIdentity()
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); FPaths::NormalizeDirectoryName(Project);
	if (!FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok")) || !GetWorld()
		|| GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityGameAppCity"))
	{ Finish(TEXT("Wrong project/map; no name input is permitted.")); return false; }
	return true;
}
bool URokChangeNameAuditSubsystem::CheckUnknownProfile()
{
	const auto* GameInstance = GetWorld()->GetGameInstance();
	const auto* Runtime = GameInstance ? GameInstance->GetSubsystem<URokUnityRuntimeStateSubsystem>() : nullptr;
	if (!Runtime || !Runtime->IsRuntimeStateLoaded()) { Finish(TEXT("Missing local runtime profile state.")); return false; }
	// Inspect availability flags only. No current user name, role ID or alliance name is read or recorded.
	const auto Profile = Runtime->GetPlayerProfileState();
	if (Profile.bHasDisplayName || Profile.bHasRoleId || Profile.bHasAlliance)
	{ Finish(TEXT("This fixture audit requires the anonymous, unsynchronized local profile.")); return false; }
	return true;
}
bool URokChangeNameAuditSubsystem::Begin()
{
	if (!CheckIdentity() || !CheckUnknownProfile()) return false;
	const TCHAR* Cursor = FCommandLine::Get(); FString Token;
	while (FParse::Token(Cursor, Token, false))
		if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
			&& ((Token.Contains(TEXT("Audit"), ESearchCase::IgnoreCase) && !Token.Equals(TEXT("-RokChangeNameAudit"), ESearchCase::IgnoreCase))
				|| Token.StartsWith(TEXT("-RokOpen"), ESearchCase::IgnoreCase) || Token.StartsWith(TEXT("-RokAuto"), ESearchCase::IgnoreCase)))
		{ Finish(TEXT("Run the name audit in a dedicated process without other audit/open/auto-route flags.")); return false; }
	if (!FSlateApplication::IsInitialized()) { Finish(TEXT("Slate is not initialized.")); return false; }
	TArray<UUserWidget*> Widgets; int32 Count = 0;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokMainHudWidget::StaticClass(), false);
	for (auto* Widget : Widgets) if (Widget->GetWorld() == GetWorld() && ChangeNameAuditVisible(Widget) && Widget->GetCachedGeometry().GetLocalSize().X > 1)
	{ Hud = Cast<URokMainHudWidget>(Widget); ++Count; }
	if (Count != 1) { Finish(TEXT("Expected one attached city HUD.")); return false; }
	Widgets.Reset(); Count = 0;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokUnityModalWidget::StaticClass(), false);
	for (auto* Widget : Widgets) if (Widget->GetWorld() == GetWorld()) { Modal = Cast<URokUnityModalWidget>(Widget); ++Count; }
	if (Count != 1 || Modal->IsModalOpen()) { Finish(TEXT("Expected one closed city modal.")); return false; }
	ExpectedFeedback = EmptyFeedback;
	return Validate();
}
bool URokChangeNameAuditSubsystem::CheckText(FName Name, const FString& Expected)
{
	const auto* Text = Cast<UTextBlock>(Modal->GetWidgetFromName(Name));
	if (!ChangeNameAuditVisible(Text) || Text->GetText().ToString() != Expected)
	{ Finish(FString::Printf(TEXT("Unexpected fixed UI label %s; text values omitted."), *Name.ToString())); return false; }
	return true;
}
bool URokChangeNameAuditSubsystem::Validate()
{
	if (!CheckIdentity() || !CheckUnknownProfile()) return false;
	if (!Hud.IsValid() || !Modal.IsValid()) { Finish(TEXT("Runtime widget host was destroyed.")); return false; }
	if (View == EView::City || View == EView::Profile)
	{
		if (PreviousFocus.IsValid() && FSlateApplication::Get().GetUserFocusedWidget(0) == PreviousFocus.Pin())
		{ Finish(TEXT("Closed name input retained actual Slate focus.")); return false; }
		if (View == EView::City)
		{
			if (Modal->IsModalOpen() || ChangeNameAuditVisible(Modal.Get()))
			{ Finish(TEXT("Final close failed to return to city.")); return false; }
			return true;
		}
		if (!Modal->IsModalOpen() || !ChangeNameAuditVisible(Modal->GetWidgetFromName(TEXT("ProfileRenameButton")))
			|| Modal->GetWidgetFromName(TEXT("ChangeNameInput")))
		{ Finish(TEXT("Expected profile after the name window close.")); return false; }
		bObservedNameRemoval = true;
		return true;
	}
	const auto* CurrentInput = Cast<UEditableTextBox>(Modal->GetWidgetFromName(TEXT("ChangeNameInput")));
	const auto* Confirm = Cast<UButton>(Modal->GetWidgetFromName(TEXT("ChangeNameConfirmButton")));
	const auto* Canvas = Modal->GetWidgetFromName(TEXT("ModalModeCanvas"));
	const auto* Blocker = Modal->GetWidgetFromName(TEXT("ModalAnimationInputBlocker"));
	if (!Modal->IsModalOpen() || !ChangeNameAuditVisible(CurrentInput) || !ChangeNameAuditVisible(Confirm)
		|| !Canvas || Confirm->GetIsEnabled() || CurrentInput->GetIsReadOnly() || !CurrentInput->GetIsEnabled()
		|| CurrentInput->GetText().ToString() != ExpectedText)
	{ Finish(TEXT("Name fixture, editable state or disabled confirmation mismatch; current text omitted.")); return false; }
	if (!CheckText(TEXT("ChangeNameCharacterCount"), FString::Printf(TEXT("%d/15"), ExpectedText.Len()))
		|| !CheckText(TEXT("ChangeNameValidationFeedback"), ExpectedFeedback)
		|| !CheckText(TEXT("ChangeNameServiceBoundaryState"), TEXT("服务器改名、权威背包与扣费状态尚未接入；不会修改本地资料。"))) return false;
	const FVector2D Scale = Canvas->GetRenderTransform().Scale;
	const float Opacity = Canvas->GetRenderOpacity();
	if (!FMath::IsNearlyEqual(Scale.X, Scale.Y, .0001)) { Finish(TEXT("T1 window XY scales diverged.")); return false; }
	if (View == EView::Opening || View == EView::Closing)
	{
		const bool bOpening = View == EView::Opening;
		if (!ChangeNameAuditVisible(Blocker) || Canvas->GetVisibility() != ESlateVisibility::HitTestInvisible
			|| !(Scale.X > (bOpening ? 1.05 : 1.0) && Scale.X <= 1.1001)
			|| (bOpening ? !FMath::IsNearlyEqual(Opacity, 1.f) : !(Opacity > 0 && Opacity < 1)))
		{ Finish(TEXT("Missed actual T1 intermediate sample or input shield; no synthetic animation advance is used.")); return false; }
		return true;
	}
	if (!Scale.Equals(FVector2D(1), .0001) || !FMath::IsNearlyEqual(Opacity, 1.f) || ChangeNameAuditVisible(Blocker))
	{ Finish(TEXT("T1 window did not settle at scale/opacity 1 with its input shield removed.")); return false; }
	const auto* Description = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("ChangeNameDescription")));
	const auto* Justification = Description ? FindFProperty<FByteProperty>(Description->GetClass(), TEXT("Justification")) : nullptr;
	if (!Input.IsValid() || CurrentInput != Input.Get() || !ChangeNameAuditVisible(Description)
		|| !ChangeNameAuditRect(Input.Get(), FVector2D(329, 270), FVector2D(782, 54))
		|| !ChangeNameAuditRect(Description, FVector2D(329, 371.9), FVector2D(782, 103.6))
		|| Input->GetWidgetStyle().TextStyle.Font.Size != 18 || Input->GetJustification() != ETextJustify::Left
		|| Description->GetFont().Size != 15 || !Justification || Justification->GetPropertyValue_InContainer(Description) != uint8(ETextJustify::Left)
		|| Description->GetClipping() != EWidgetClipping::ClipToBounds
		|| Input->GetHintText().ToString() != TEXT("点击此处输入城主昵称..."))
	{ Finish(TEXT("Source-derived Chinese input/description geometry or text style mismatch.")); return false; }
	if (!CheckText(TEXT("ChangeNameDescription"), TEXT("您能够输入3至15个字符，可以由数字、字母和特殊符号组成"))) return false;
	if (bCheckFocus && (!FocusedWidget.IsValid() || FSlateApplication::Get().GetUserFocusedWidget(0) != FocusedWidget.Pin()
		|| !(Input->HasKeyboardFocus() || Input->HasUserFocusedDescendants(GetWorld()->GetFirstPlayerController()))))
	{ Finish(TEXT("Editing the fixed name fixture lost actual Slate input focus.")); return false; }
	return true;
}

bool URokChangeNameAuditSubsystem::BindInput(bool bReopened)
{
	Input = Cast<UEditableTextBox>(Modal->GetWidgetFromName(TEXT("ChangeNameInput")));
	if (!Input.IsValid() || (bReopened && !bObservedNameRemoval))
	{ Finish(TEXT("Name input is absent or its preceding removal was not observed.")); return false; }
	// ConstructWidget uses NewObject with a fixed name and outer. UE may reconstruct
	// that UObject in place, retaining its index/serial. Identity is not a reset test.
	// Validate instead requires the empty draft, 0/15, empty feedback and disabled
	// confirmation; later stages focus the actual Slate input and type a new fixture.
	if (!Validate()) return false;
	if (bReopened)
	{
		auto Action = MakeShared<FJsonObject>();
		Action->SetBoolField(TEXT("reopen_previous_input_removed"), bObservedNameRemoval);
		Action->SetBoolField(TEXT("reopen_uobject_identity_reused"), Input == PreviousInput);
		Action->SetBoolField(TEXT("reopen_empty_draft_validated"), ExpectedText.IsEmpty());
		Action->SetNumberField(TEXT("frame"), double(Frame)); Actions.Add(MakeShared<FJsonValueObject>(Action));
	}
	return true;
}
bool URokChangeNameAuditSubsystem::BindFocus(bool bReopened)
{
	FocusedWidget = FSlateApplication::Get().GetUserFocusedWidget(0); bCheckFocus = true;
	if (!FocusedWidget.IsValid() || FocusedWidget.Pin()->GetType() != FName(TEXT("SEditableText"))
		|| (bReopened && PreviousFocus.IsValid() && FocusedWidget.Pin() == PreviousFocus.Pin()))
	{ Finish(TEXT("Expected fresh focused Slate editable text after focus request.")); return false; }
	return Validate();
}
bool URokChangeNameAuditSubsystem::Click(UUserWidget* Owner, FName Name)
{
	// Explicit allowlist prevents future stage edits from accidentally invoking submit.
	if (Name != TEXT("PlayerProfileHitButton") && Name != TEXT("ProfileRenameButton")
		&& Name != TEXT("TypeMidCloseButton") && Name != TEXT("Type1CloseButton"))
	{ Finish(TEXT("Audit attempted a button outside its navigation-only allowlist.")); return false; }
	auto* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Name)) : nullptr;
	if (!Button || Owner->GetWorld() != GetWorld() || !ChangeNameAuditVisible(Button) || !Button->GetIsEnabled()
		|| Button->GetCachedGeometry().GetLocalSize().X <= 0
		|| (Owner == Modal.Get() && ChangeNameAuditVisible(Modal->GetWidgetFromName(TEXT("ModalAnimationInputBlocker")))))
	{ Finish(FString::Printf(TEXT("Navigation button unavailable: %s."), *Name.ToString())); return false; }
	auto Action = MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button_delegate"), Name.ToString());
	Action->SetNumberField(TEXT("frame"), double(Frame)); Actions.Add(MakeShared<FJsonValueObject>(Action));
	Button->OnClicked.Broadcast(); return true;
}
bool URokChangeNameAuditSubsystem::EditFixture(const FString& Fixture, const FString& Result, const FString& Feedback, bool bKeyboard)
{
	if (!Validate() || !bCheckFocus || !FocusedWidget.IsValid()) return false;
	ExpectedText = Result; ExpectedFeedback = Feedback;
	if (bKeyboard)
	{
		auto& Slate = FSlateApplication::Get();
		const FModifierKeysState Control(false, false, true, false, false, false, false, false, false), Plain;
		bool Handled = Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::A, Control, uint32(0), false, 0, 0));
		Slate.ProcessKeyUpEvent(FKeyEvent(EKeys::A, Control, uint32(0), false, 0, 0));
		if (Fixture.IsEmpty())
		{
			Handled &= Slate.ProcessKeyDownEvent(FKeyEvent(EKeys::BackSpace, Plain, uint32(0), false, 0, 0));
			Slate.ProcessKeyUpEvent(FKeyEvent(EKeys::BackSpace, Plain, uint32(0), false, 0, 0));
		}
		else for (TCHAR Character : Fixture) Handled &= Slate.ProcessKeyCharEvent(FCharacterEvent(Character, Plain, uint32(0), false));
		if (!Handled) { Finish(TEXT("Fixed Slate key events were not handled.")); return false; }
	}
	else
	{
		// Public Slate SetText fires the actual UMG bound OnTextChanged. UMG SetText
		// itself preassigns Text and can suppress that notification. This is a fixture, not typing.
		StaticCastSharedPtr<SEditableText>(FocusedWidget.Pin())->SetText(FText::FromString(Fixture));
	}
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("input_method"), bKeyboard ? TEXT("Slate Ctrl+A and fixed character/backspace events") : TEXT("Public SEditableText::SetText fixture and actual OnTextChanged binding"));
	Action->SetStringField(TEXT("fixed_fixture"), Fixture); Action->SetStringField(TEXT("expected_sanitized_fixture"), Result);
	Action->SetNumberField(TEXT("frame"), double(Frame)); Actions.Add(MakeShared<FJsonValueObject>(Action));
	Enter(Step + 1); return true;
}
void URokChangeNameAuditSubsystem::PrepareClose()
{
	PreviousInput = Input; PreviousFocus = FocusedWidget; bCheckFocus = false; bObservedNameRemoval = false;
}
void URokChangeNameAuditSubsystem::Enter(int32 NextStep) { Step = NextStep; StepStarted = Elapsed; }
TSharedRef<FJsonObject> URokChangeNameAuditSubsystem::State() const
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("step"), Step); Result->SetNumberField(TEXT("frame"), double(Frame));
	Result->SetNumberField(TEXT("time_seconds"), Elapsed); Result->SetBoolField(TEXT("focus_asserted"), bCheckFocus);
	// Never serialize arbitrary current text; only the hardcoded expected fixture and equality verdict.
	Result->SetStringField(TEXT("expected_fixture"), ExpectedText); Result->SetNumberField(TEXT("expected_character_count"), ExpectedText.Len());
	if (Modal.IsValid())
	{
		if (const auto* Canvas = Modal->GetWidgetFromName(TEXT("ModalModeCanvas")))
		{
			Result->SetNumberField(TEXT("actual_window_scale"), Canvas->GetRenderTransform().Scale.X);
			Result->SetNumberField(TEXT("actual_window_opacity"), Canvas->GetRenderOpacity());
		}
		if (const auto* Button = Cast<UButton>(Modal->GetWidgetFromName(TEXT("ChangeNameConfirmButton"))))
			Result->SetBoolField(TEXT("confirm_enabled"), Button->GetIsEnabled());
	}
	return Result;
}
void URokChangeNameAuditSubsystem::Capture(const TCHAR* Name)
{
	if (!Validate()) return;
	if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Another screenshot is already pending.")); return; }
	PendingPath = Directory / (FString(Name) + TEXT(".png")); PendingCapture = MakeShared<FJsonObject>();
	PendingCapture->SetStringField(TEXT("name"), Name); PendingCapture->SetStringField(TEXT("path"), PendingPath);
	PendingCapture->SetObjectField(TEXT("state_at_request"), State()); ScreenshotStarted = Elapsed;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}
void URokChangeNameAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !PendingCapture.IsValid() || !Validate()) return;
	TArray<uint8> Bytes; const uint8 Signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
	if (!FFileHelper::LoadFileToArray(Bytes, *PendingPath) || Bytes.Num() < 24 || FMemory::Memcmp(Bytes.GetData(), Signature, 8) != 0)
	{ Finish(TEXT("Processed screenshot is absent, empty or not PNG.")); return; }
	const auto ReadBE = [&Bytes](int32 Offset) { return (uint32(Bytes[Offset]) << 24) | (uint32(Bytes[Offset + 1]) << 16) | (uint32(Bytes[Offset + 2]) << 8) | uint32(Bytes[Offset + 3]); };
	if (ReadBE(16) < 2 || ReadBE(20) < 2) { Finish(TEXT("Invalid screenshot dimensions.")); return; }
	PendingCapture->SetNumberField(TEXT("file_bytes"), Bytes.Num());
	PendingCapture->SetNumberField(TEXT("width"), ReadBE(16)); PendingCapture->SetNumberField(TEXT("height"), ReadBE(20));
	PendingCapture->SetObjectField(TEXT("state_at_processed"), State());
	Captures.Add(MakeShared<FJsonValueObject>(PendingCapture)); PendingCapture.Reset();
	if (!Save(false)) { Finish(TEXT("Progress manifest save failed.")); return; }
	Enter(Step + 1);
}
void URokChangeNameAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !GetWorld()->HasBegunPlay()) return;
	Elapsed += FMath::Max(DeltaTime, 0.f); ++Frame;
	if (Elapsed > 90) { Finish(TEXT("Name audit timed out.")); return; }
	if (Modal.IsValid())
		if (const auto* Confirm = Cast<UButton>(Modal->GetWidgetFromName(TEXT("ChangeNameConfirmButton"))); Confirm && Confirm->GetIsEnabled())
		{ Finish(TEXT("Confirmation became enabled during an audit tick.")); return; }
	if (PendingCapture.IsValid()) { if (Elapsed - ScreenshotStarted > 10) Finish(TEXT("Screenshot callback timed out.")); return; }
	const double Delay = Step == 0 ? 3.0 : (Step == 4 || Step == 20 ? .05 : .5);
	if (Elapsed - StepStarted < Delay) return;
	switch (Step)
	{
	case 0: if (Begin()) Capture(TEXT("01_city_before")); break;
	case 1: if (Click(Hud.Get(), TEXT("PlayerProfileHitButton"))) { View = EView::Profile; Enter(2); } break;
	case 2: Capture(TEXT("02_profile")); break;
	case 3: if (Click(Modal.Get(), TEXT("ProfileRenameButton"))) { View = EView::Opening; Enter(4); } break;
	case 4: Capture(TEXT("03_t1_opening")); break;
	case 5: View = EView::Name; if (BindInput(false)) Capture(TEXT("04_empty_open")); break;
	case 6: Input->SetKeyboardFocus(); Enter(7); break;
	case 7: if (BindFocus(false)) EditFixture(TEXT("ab"), TEXT("ab"), TEXT("名称至少需要 3 个字符。"), true); break;
	case 8: Capture(TEXT("05_keyboard_short")); break;
	case 9: EditFixture(TEXT("RokAudit"), TEXT("RokAudit"), ValidFeedback, false); break;
	case 10: Capture(TEXT("06_valid_local_only")); break;
	case 11: EditFixture(TEXT("12345"), TEXT("12345"), TEXT("名称不能只由数字组成。"), false); break;
	case 12: Capture(TEXT("07_digits_invalid")); break;
	case 13: EditFixture(TEXT(""), TEXT(""), EmptyFeedback, true); break;
	case 14: Capture(TEXT("08_keyboard_empty")); break;
	case 15: EditFixture(TEXT("RokAudit1234567"), TEXT("RokAudit1234567"), ValidFeedback, false); break;
	case 16: Capture(TEXT("09_maximum_15")); break;
	case 17: EditFixture(TEXT("RokAudit12345678"), TEXT("RokAudit1234567"), ValidFeedback, false); break;
	case 18: Capture(TEXT("10_long_fixture_clamped")); break;
	case 19: PrepareClose(); if (Click(Modal.Get(), TEXT("TypeMidCloseButton"))) { View = EView::Closing; Enter(20); } break;
	case 20: Capture(TEXT("11_t1_closing")); break;
	case 21: View = EView::Profile; Capture(TEXT("12_profile_returned")); break;
	case 22: if (Click(Modal.Get(), TEXT("ProfileRenameButton"))) { ExpectedText.Reset(); ExpectedFeedback = EmptyFeedback; View = EView::Name; Enter(23); } break;
	case 23: if (BindInput(true)) Capture(TEXT("13_reopen_empty")); break;
	case 24: Input->SetKeyboardFocus(); Enter(25); break;
	case 25: if (BindFocus(true)) EditFixture(TEXT("RokAgain"), TEXT("RokAgain"), ValidFeedback, true); break;
	case 26: Capture(TEXT("14_reopen_keyboard_valid")); break;
	case 27: PrepareClose(); if (Click(Modal.Get(), TEXT("TypeMidCloseButton"))) { View = EView::Profile; Enter(28); } break;
	case 28: if (Validate() && Click(Modal.Get(), TEXT("Type1CloseButton"))) { View = EView::City; Enter(29); } break;
	case 29: Capture(TEXT("15_city_after")); break;
	case 30: Finish(); break;
	default: Finish(TEXT("Unknown name-audit stage.")); break;
	}
}
bool URokChangeNameAuditSubsystem::Save(bool bComplete)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("complete"), bComplete); Root->SetBoolField(TEXT("passed"), bComplete && Failures.IsEmpty() && Captures.Num() == ChangeNameAuditCaptureCount);
	Root->SetNumberField(TEXT("audit_version"), 2); Root->SetNumberField(TEXT("expected_capture_count"), ChangeNameAuditCaptureCount);
	Root->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Root->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName());
	Root->SetStringField(TEXT("input_method"), TEXT("Existing navigation UMG OnClicked delegates. Focused Slate Ctrl+A/character/backspace events and public SEditableText::SetText fixtures use the actual UMG OnTextChanged binding. No physical typing, pointer hit-testing or clipboard claim."));
	Root->SetStringField(TEXT("scope"), TEXT("Anonymous local profile only, fixed ASCII fixtures; no current user text is recorded. Confirm stays disabled and is never invoked. No rename request, service response, inventory/fee or name availability claim. Real NativeTick T1 intermediate and settled captures; no synthetic advance."));
	Root->SetStringField(TEXT("source"), TEXT("UIPlayerData/UI_Win_ChangeName.prefab; ChangeNameMediator; shared UA_WinAnime_T1_On/Off"));
	Root->SetBoolField(TEXT("unity_visual_comparison_complete"), false);
	Root->SetArrayField(TEXT("actions"), Actions); Root->SetArrayField(TEXT("captures"), Captures);
	TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error)); Root->SetArrayField(TEXT("failures"), Errors);
	FString Json; if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json))) return false;
	return FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokChangeNameAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	if (Failures.IsEmpty() && Captures.Num() != ChangeNameAuditCaptureCount) Failures.Add(TEXT("Expected exactly 15 processed screenshots."));
	const bool bPassed = Failures.IsEmpty();
	if (!Save(bPassed)) Failures.Add(TEXT("Final manifest save failed."));
	bFinished = true;
	UE_LOG(LogTemp, Display, TEXT("Rok ChangeName audit passed=%d captures=%d failures=%d output=%s"), Failures.IsEmpty(), Captures.Num(), Failures.Num(), *Directory);
	for (const auto& Error : Failures) UE_LOG(LogTemp, Error, TEXT("Rok ChangeName audit: %s"), *Error);
	FPlatformMisc::RequestExit(false);
}
