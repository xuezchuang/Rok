#include "RokPlayerHeadAuditSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokMainHudWidget.h"
#include "RokUnityModalWidget.h"
#include "RokPlayerHeadCatalog.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
	bool HeadAuditVisible(const UWidget* Widget)
	{
		if (!Widget || !Widget->IsVisible()) return false;
		for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
			if (!Parent->IsVisible()) return false;
		return true;
	}
}

bool URokPlayerHeadAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokPlayerHeadAudit"))
		&& !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokPlayerHeadAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokPlayerHeadAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/PlayerHeadAudit") /
		FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokPlayerHeadAuditSubsystem::ScreenshotProcessed);
}
void URokPlayerHeadAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	Super::Deinitialize();
}
TStatId URokPlayerHeadAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokPlayerHeadAuditSubsystem, STATGROUP_Tickables); }

bool URokPlayerHeadAuditSubsystem::FindWidgets()
{
	TArray<UUserWidget*> Found;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, URokMainHudWidget::StaticClass(), false);
	int32 HudCount = 0;
	for (auto* Widget : Found) if (Widget->GetWorld() == GetWorld() && HeadAuditVisible(Widget) && Widget->GetCachedGeometry().GetLocalSize().X > 1)
	{ Hud = Cast<URokMainHudWidget>(Widget); ++HudCount; }
	Found.Reset();
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, URokUnityModalWidget::StaticClass(), false);
	int32 ModalCount = 0;
	for (auto* Widget : Found) if (Widget->GetWorld() == GetWorld()) { Modal = Cast<URokUnityModalWidget>(Widget); ++ModalCount; }
	if (HudCount != 1 || ModalCount != 1) return false;
	if (Modal->IsModalOpen()) { Finish(TEXT("Modal already open before audit; refusing to replace it.")); return false; }
	return true;
}
void URokPlayerHeadAuditSubsystem::Next() { ++Step; StepStarted = Elapsed; }
bool URokPlayerHeadAuditSubsystem::Click(UUserWidget* Owner, const TCHAR* Name)
{
	auto* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Name)) : nullptr;
	if (!Owner || Owner->GetWorld() != GetWorld() || !HeadAuditVisible(Owner) || !HeadAuditVisible(Button)
		|| !Button->GetIsEnabled() || Button->GetCachedGeometry().GetLocalSize().X <= 0)
	{ Finish(FString::Printf(TEXT("Missing/hidden/disabled/unlaid-out runtime button: %s"), Name)); return false; }
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("button"), Name); Action->SetStringField(TEXT("event"), TEXT("OnClicked delegate, not physical input"));
	Action->SetNumberField(TEXT("frame"), double(Frame)); Actions.Add(MakeShared<FJsonValueObject>(Action));
	Button->OnClicked.Broadcast();
	Next(); return true;
}
TSharedRef<FJsonObject> URokPlayerHeadAuditSubsystem::State() const
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("frame"), double(Frame));
	Result->SetBoolField(TEXT("modal_open"), Modal.IsValid() && Modal->IsModalOpen());
	if (Modal.IsValid())
	{
		const UWidget* Tip = Modal->GetWidgetFromName(TEXT("PlayerKillTipRoot"));
		Result->SetBoolField(TEXT("kill_tip_present"), Tip != nullptr);
		if (Tip) Result->SetNumberField(TEXT("kill_tip_scale"), Tip->GetRenderTransform().Scale.X);
		const UWidget* ActionTip = Modal->GetWidgetFromName(TEXT("PlayerActionTipRoot"));
		Result->SetBoolField(TEXT("action_tip_present"), ActionTip != nullptr);
		if (ActionTip) Result->SetNumberField(TEXT("action_tip_scale"), ActionTip->GetRenderTransform().Scale.X);
		if (const auto* Recovery = Cast<URichTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerActionTipRecovery"))))
			Result->SetStringField(TEXT("action_recovery_text"), Recovery->GetText().ToString());
		TArray<TSharedPtr<FJsonValue>> TierCounts;
		for (int32 Tier = 1; Tier <= 5; ++Tier)
		{
			const auto* Count = Cast<UTextBlock>(Modal->GetWidgetFromName(*FString::Printf(TEXT("PlayerKillTipTierCount_%d"), Tier)));
			if (Count) TierCounts.Add(MakeShared<FJsonValueString>(Count->GetText().ToString()));
		}
		Result->SetArrayField(TEXT("kill_tip_counts"), TierCounts);
		if (const auto* Scroll = Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("PlayerHeadGridScroll"))))
		{ Result->SetNumberField(TEXT("scroll_offset"), Scroll->GetScrollOffset()); Result->SetNumberField(TEXT("scroll_end"), Scroll->GetScrollOffsetOfEnd()); }
		if (const auto* Name = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerHeadPreviewName")))) Result->SetStringField(TEXT("selected_name"), Name->GetText().ToString());
		if (const auto* Confirm = Cast<UButton>(Modal->GetWidgetFromName(TEXT("PlayerHeadConfirmButton")))) Result->SetBoolField(TEXT("confirm_enabled"), Confirm->GetIsEnabled());
		Result->SetBoolField(TEXT("missing_art_notice"), Modal->GetWidgetFromName(TEXT("PlayerHeadPreviewMissingArt")) != nullptr);
	}
	return Result;
}
void URokPlayerHeadAuditSubsystem::Capture(const TCHAR* Name)
{
	PendingPath = Directory / (FString(Name) + TEXT(".png"));
	PendingCapture = MakeShared<FJsonObject>();
	PendingCapture->SetStringField(TEXT("path"), PendingPath);
	PendingCapture->SetObjectField(TEXT("state_at_request"), State());
	ScreenshotStarted = Elapsed;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}
void URokPlayerHeadAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !PendingCapture.IsValid()) return;
	const int64 Size = IFileManager::Get().FileSize(*PendingPath);
	if (Size <= 0) { Finish(TEXT("Screenshot processed without a nonempty output file.")); return; }
	PendingCapture->SetNumberField(TEXT("file_bytes"), double(Size));
	PendingCapture->SetObjectField(TEXT("state_at_processed"), State());
	Captures.Add(MakeShared<FJsonValueObject>(PendingCapture)); PendingCapture.Reset();
	if (Step == 13 || Step == 20)
	{
		const UWidget* Tip = Modal.IsValid() ? Modal->GetWidgetFromName(
			Step == 13 ? TEXT("PlayerKillTipRoot") : TEXT("PlayerActionTipRoot")) : nullptr;
		if (!Tip || Tip->GetRenderTransform().Scale.X <= 0 || Tip->GetRenderTransform().Scale.X >= 1)
		{ Finish(TEXT("Tooltip midpoint request did not remain intermediate through screenshot processing.")); return; }
	}
	Next();
	if (!WriteManifest(false)) Finish(TEXT("Incremental manifest serialization/write failed."));
}
bool URokPlayerHeadAuditSubsystem::WriteManifest(bool Complete)
{
	auto Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("complete"), Complete); Root->SetBoolField(TEXT("passed"), Complete && Failures.IsEmpty());
	Root->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Root->SetStringField(TEXT("world"), GetWorld()->GetOutermost()->GetName());
	Root->SetStringField(TEXT("input"), TEXT("Actual HUD/UMG delegates and ScrollToEnd, real runtime ticks; not physical mouse input"));
	Root->SetBoolField(TEXT("unity_visual_comparison_complete"), false);
	Root->SetNumberField(TEXT("expected_capture_count"), FParse::Param(FCommandLine::Get(), TEXT("RokPlayerActionTipAudit")) ? 12 : 9);
	Root->SetNumberField(TEXT("scroll_before_selection"), ScrollBeforeSelection);
	Root->SetArrayField(TEXT("captures"), Captures); Root->SetArrayField(TEXT("actions"), Actions);
	TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error));
	Root->SetArrayField(TEXT("failures"), Errors);
	FString Json;
	if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json)))
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerHead audit manifest serialization failed."));
		return false;
	}
	if (!FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json"))))
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerHead audit manifest write failed: %s"), *Directory);
		return false;
	}
	return true;
}
void URokPlayerHeadAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	const int32 ExpectedCaptures = FParse::Param(FCommandLine::Get(), TEXT("RokPlayerActionTipAudit")) ? 12 : 9;
	if (Failure.IsEmpty() && Captures.Num() != ExpectedCaptures) Failures.Add(TEXT("Missing processed runtime captures."));
	bFinished = true;
	if (!WriteManifest(true)) Failures.Add(TEXT("Final manifest serialization/write failed; acceptance report unavailable."));
	UE_LOG(LogTemp, Display, TEXT("Rok PlayerHead audit finished: passed=%d output=%s failure=%s"), Failures.IsEmpty(), *Directory, *Failure);
	FGenericPlatformMisc::RequestExit(false);
}
void URokPlayerHeadAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !GetWorld()->HasBegunPlay()) return;
	if (!bIdentityVerified)
	{
		FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		FPaths::NormalizeDirectoryName(Project);
		const FString Map = GetWorld()->GetOutermost()->GetName();
		if (!FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok")) || Map != TEXT("/Game/Maps/L_RokUnityGameAppCity"))
		{
			Finish(FString::Printf(TEXT("Wrong project/map after BeginPlay: project=%s world=%s"), *Project, *Map));
			return;
		}
		bIdentityVerified = true;
	}
	Elapsed += FMath::Max(DeltaTime, 0.0f); ++Frame;
	if (Elapsed > 60) { Finish(TEXT("PlayerHead audit timed out.")); return; }
	if (PendingCapture.IsValid()) { if (Elapsed - ScreenshotStarted > 10) Finish(TEXT("Screenshot processing timed out.")); return; }
	// The tooltip reaches full scale at 7/60 seconds. Observe its live widget
	// before that deadline rather than advancing or freezing its animation.
	if (Elapsed - StepStarted < ((Step == 13 || Step == 20) ? 0.025 : 0.1)) return;
	switch (Step)
	{
	case 0: if (Elapsed > 2 && FindWidgets()) Next(); break;
	case 1: Capture(TEXT("01_city_before")); break;
	case 2: Click(Hud.Get(), TEXT("PlayerProfileHitButton")); break;
	case 3: if (Elapsed - StepStarted > 0.5) Click(Modal.Get(), TEXT("ProfilePictureButton")); break;
	case 4: if (Elapsed - StepStarted > 0.5) Capture(TEXT("02_heads_open")); break;
	case 5: Click(Modal.Get(), TEXT("PlayerHeadPageButton_1")); break;
	case 6: Capture(TEXT("03_frames_before_scroll")); break;
	case 7:
	{
		auto* Scroll = Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("PlayerHeadGridScroll")));
		if (!Scroll || Scroll->GetScrollOffsetOfEnd() <= 0) { Finish(TEXT("Frame grid is not scrollable after layout.")); return; }
		Scroll->ScrollToEnd(); Next(); break;
	}
	case 8:
	{
		auto* Scroll = Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("PlayerHeadGridScroll")));
		if (!Scroll || Scroll->GetScrollOffset() <= 0 || !FMath::IsNearlyEqual(Scroll->GetScrollOffset(), Scroll->GetScrollOffsetOfEnd(), 1.0f))
		{ Finish(TEXT("ScrollToEnd did not reach the last frame row on the next runtime ticks.")); return; }
		ScrollBeforeSelection = Scroll->GetScrollOffset(); Capture(TEXT("04_frames_scrolled")); break;
	}
	case 9: Click(Modal.Get(), TEXT("PlayerHeadChoiceButton_15")); break;
	case 10:
	{
		auto* Scroll = Cast<UScrollBox>(Modal->GetWidgetFromName(TEXT("PlayerHeadGridScroll")));
		auto* Name = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerHeadPreviewName")));
		auto* Confirm = Cast<UButton>(Modal->GetWidgetFromName(TEXT("PlayerHeadConfirmButton")));
		TArray<FRokPlayerHeadRow> Rows; FString Error;
		if (!RokPlayerHeadCatalog::Load(Rows, Error)) { Finish(Error); return; }
		const auto* Row = Rows.FindByPredicate([](const auto& Item) { return Item.Id == 2015; });
		if (!Row || !Scroll || !Name || !Confirm || Confirm->GetIsEnabled() || Name->GetText().ToString() != Row->Name
			|| !FMath::IsNearlyEqual(Scroll->GetScrollOffset(), ScrollBeforeSelection, 1.0f)
			|| (!Row->bArtAvailable && !Modal->GetWidgetFromName(TEXT("PlayerHeadPreviewMissingArt"))))
		{ Finish(TEXT("Last-frame selection failed name/art/disabled-apply/scroll-preservation checks.")); return; }
		Capture(TEXT("05_last_frame_selected")); break;
	}
	case 11: Click(Modal.Get(), TEXT("Type1CloseButton")); break;
	case 12: if (Elapsed - StepStarted > 0.5) Click(Modal.Get(), TEXT("ProfileKillsHelpButton")); break;
	case 13:
	{
		const UWidget* Tip = Modal->GetWidgetFromName(TEXT("PlayerKillTipRoot"));
		if (!Tip) { Finish(TEXT("Kills help did not create the runtime tooltip.")); return; }
		const float Scale = Tip->GetRenderTransform().Scale.X;
		if (Scale <= 0) break;
		if (Scale >= 1) { Finish(TEXT("Real runtime frame skipped tooltip entry midpoint; no fabricated intermediate capture.")); return; }
		Capture(TEXT("06_kill_tip_entering")); break;
	}
	case 14:
	{
		if (Elapsed - StepStarted < 0.25) break;
		const UWidget* Tip = Modal->GetWidgetFromName(TEXT("PlayerKillTipRoot"));
		if (!Tip || !FMath::IsNearlyEqual(Tip->GetRenderTransform().Scale.X, 1.0))
		{ Finish(TEXT("Kills tooltip failed to settle at unit scale.")); return; }
		for (int32 Tier = 1; Tier <= 5; ++Tier)
		{
			const auto* Count = Cast<UTextBlock>(Modal->GetWidgetFromName(*FString::Printf(TEXT("PlayerKillTipTierCount_%d"), Tier)));
			if (!Count || Count->GetText().ToString() != TEXT("—"))
			{ Finish(TEXT("Kills tooltip must expose all five unknown server counts.")); return; }
		}
		Capture(TEXT("07_kill_tip_open")); break;
	}
	case 15: Click(Modal.Get(), TEXT("PlayerKillTipClickAway")); break;
	case 16:
		if (!Modal->IsModalOpen() || Modal->GetWidgetFromName(TEXT("PlayerKillTipRoot"))
			|| !Modal->GetWidgetFromName(TEXT("ProfilePictureButton"))) Finish(TEXT("Click-away did not preserve profile and remove tooltip."));
		else Capture(TEXT("08_profile_after_tip"));
		break;
	case 17:
		if (FParse::Param(FCommandLine::Get(), TEXT("RokPlayerActionTipAudit")))
		{
			Step = 19;
			Click(Modal.Get(), TEXT("ProfileActionHelpButton"));
		}
		else Click(Modal.Get(), TEXT("Type1CloseButton"));
		break;
	case 18:
		if (Elapsed - StepStarted > 0.5) { if (Modal->IsModalOpen()) Finish(TEXT("Profile did not close.")); else Capture(TEXT("09_city_after")); } break;
	case 19: Finish(); break;
	case 20:
	{
		const UWidget* Tip = Modal->GetWidgetFromName(TEXT("PlayerActionTipRoot"));
		if (!Tip) { Finish(TEXT("Action help did not create the runtime tooltip.")); return; }
		const float Scale = Tip->GetRenderTransform().Scale.X;
		if (Scale <= 0) break;
		if (Scale >= 1) { Finish(TEXT("Real runtime frame skipped action tooltip midpoint.")); return; }
		Capture(TEXT("09_action_tip_entering")); break;
	}
	case 21:
	{
		if (Elapsed - StepStarted < 0.25) break;
		const UWidget* Tip = Modal->GetWidgetFromName(TEXT("PlayerActionTipRoot"));
		if (!Tip || !FMath::IsNearlyEqual(Tip->GetRenderTransform().Scale.X, 1.0))
		{ Finish(TEXT("Action tooltip failed to settle at unit scale.")); return; }
		const auto* Recovery = Cast<URichTextBlock>(Modal->GetWidgetFromName(TEXT("PlayerActionTipRecovery")));
		if (!Recovery || Recovery->GetText().ToString() != TEXT("行动力恢复1点需要—秒\n（恢复速度提升：+ —%）\n\n全部恢复还需   <Remaining>—</>"))
		{ Finish(TEXT("Action tooltip must retain unknown recovery, bonus, and full-recovery time.")); return; }
		for (const TCHAR* Name : {TEXT("PlayerActionTipDescription"), TEXT("PlayerActionTipWarning")})
			if (!Modal->GetWidgetFromName(Name)) { Finish(TEXT("Action tooltip is missing a source text region.")); return; }
		Capture(TEXT("10_action_tip_open")); break;
	}
	case 22: Click(Modal.Get(), TEXT("PlayerActionTipClickAway")); break;
	case 23:
		if (!Modal->IsModalOpen() || Modal->GetWidgetFromName(TEXT("PlayerActionTipRoot"))
			|| !Modal->GetWidgetFromName(TEXT("ProfilePictureButton"))) Finish(TEXT("Action tooltip click-away did not preserve the profile."));
		else Capture(TEXT("11_profile_after_action_tip"));
		break;
	case 24: Click(Modal.Get(), TEXT("Type1CloseButton")); break;
	case 25:
		if (Elapsed - StepStarted > 0.5)
		{
			if (Modal->IsModalOpen()) Finish(TEXT("Profile did not close after action tooltip."));
			else Capture(TEXT("12_city_after_action_tip"));
		}
		break;
	case 26: Finish(); break;
	}
}
