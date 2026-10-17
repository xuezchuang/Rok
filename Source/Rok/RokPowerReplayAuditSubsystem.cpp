#include "RokPowerReplayAuditSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokMainHudWidget.h"
#include "RokPowerGainWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
bool IsPowerReplayCityWorld(const UWorld* World)
{
	return World && World->GetOutermost()->GetName() == TEXT("/Game/Maps/L_RokUnityGameAppCity");
}
}

bool URokPowerReplayAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	// Startup /Temp/Untitled_0 is a Game world too; its teardown must not exit the real audit process.
	return IsPowerReplayCityWorld(Cast<UWorld>(Outer))
		&& FParse::Param(FCommandLine::Get(), TEXT("RokPowerReplayAudit")) && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokPowerReplayAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
TStatId URokPowerReplayAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokPowerReplayAuditSubsystem, STATGROUP_Tickables); }
void URokPowerReplayAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bOwnsTargetWorld = IsPowerReplayCityWorld(GetWorld());
	if (!bOwnsTargetWorld) return;
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/PowerReplayAudit") / FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	FParse::Value(FCommandLine::Get(), TEXT("RokPowerReplayCaptureTime="), CaptureTime);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokPowerReplayAuditSubsystem::Processed);
}
void URokPowerReplayAuditSubsystem::Deinitialize()
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	// Capture ownership at initialization, so actual city travel/teardown still fails even before its first tick.
	if (bOwnsTargetWorld && !bFinished) Finish(TEXT("Target city world ended before audit finished."));
	Super::Deinitialize();
}
TSharedRef<FJsonObject> URokPowerReplayAuditSubsystem::Identity() const
{
	auto Result = MakeShared<FJsonObject>();
	const FString RawProject = FPaths::ProjectDir();
	const FString FullProject = FPaths::ConvertRelativePathToFull(RawProject);
	FString Project = FullProject, Expected = TEXT("D:/ueDemo/Rok");
	// UE IsSamePath only removes duplicate slashes; directory trailing slashes remain significant.
	FPaths::NormalizeDirectoryName(Project); FPaths::NormalizeDirectoryName(Expected);
	Result->SetStringField(TEXT("project_raw"), RawProject);
	Result->SetStringField(TEXT("project_full"), FullProject);
	Result->SetStringField(TEXT("project_normalized"), Project);
	Result->SetStringField(TEXT("expected_project_normalized"), Expected);
	Result->SetBoolField(TEXT("project_matches"), FPaths::IsSamePath(Project, Expected));
	const UWorld* World = GetWorld();
	Result->SetStringField(TEXT("world_package"), World ? World->GetOutermost()->GetName() : TEXT("<null>"));
	Result->SetBoolField(TEXT("city_matches"), IsPowerReplayCityWorld(World));
	Result->SetBoolField(TEXT("owns_target_world"), bOwnsTargetWorld);
	Result->SetBoolField(TEXT("paused"), World && World->IsPaused());
	Result->SetNumberField(TEXT("effective_time_dilation"), World && World->GetWorldSettings() ? World->GetWorldSettings()->GetEffectiveTimeDilation() : -1);
	Result->SetStringField(TEXT("next_url"), World ? World->NextURL : TEXT("<null>"));
	return Result;
}
TSharedRef<FJsonObject> URokPowerReplayAuditSubsystem::State(const FString& Kind) const
{
	auto Row = MakeShared<FJsonObject>();
	Row->SetStringField(TEXT("kind"), Kind); Row->SetNumberField(TEXT("frame"), static_cast<double>(GFrameCounter));
	Row->SetNumberField(TEXT("elapsed"), Elapsed - Origin);
	Row->SetNumberField(TEXT("world_seconds"), GetWorld() ? GetWorld()->GetTimeSeconds() : 0);
	Row->SetNumberField(TEXT("wall_elapsed"), FPlatformTime::Seconds() - WallOrigin);
	Row->SetNumberField(TEXT("notifications"), Notification);
	if (Hud.IsValid()) if (const auto* Text = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("PlayerPowerText")))) Row->SetStringField(TEXT("hud_text"), Text->GetText().ToString());
	if (Effect.IsValid())
	{
		Row->SetBoolField(TEXT("playing"), Effect->IsPlaying());
		Row->SetNumberField(TEXT("remaining"), Effect->GetRemainingTime());
		Row->SetNumberField(TEXT("mesh_time"), Effect->GetPlaybackTime());
		TArray<TSharedPtr<FJsonValue>> Particles;
#if !UE_BUILD_SHIPPING
		TArray<float> Ages; TArray<bool> Pending; Effect->GetParticleStateForAudit(Ages, Pending);
		for (int32 I = 0; I < Ages.Num(); ++I) { auto P = MakeShared<FJsonObject>(); P->SetNumberField(TEXT("age"), Ages[I]); P->SetBoolField(TEXT("pending"), Pending[I]); Particles.Add(MakeShared<FJsonValueObject>(P)); }
#endif
		Row->SetArrayField(TEXT("particles"), Particles);
	}
	return Row;
}
void URokPowerReplayAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !bOwnsTargetWorld) return;
	Elapsed += DeltaTime;
	if (Elapsed > 25) { Finish(TEXT("Timeout.")); return; }
	if (!bStarted && Elapsed < 3) return;
	const auto ActualIdentity = Identity();
	if (!ActualIdentity->GetBoolField(TEXT("city_matches"))) { Finish(TEXT("Requires actual city map.")); return; }
	if (!ActualIdentity->GetBoolField(TEXT("project_matches"))) { Finish(TEXT("Project directory identity mismatch; see identity fields.")); return; }
	if (ActualIdentity->GetBoolField(TEXT("paused"))) { Finish(TEXT("Target city is paused.")); return; }
	if (!FMath::IsNearlyEqual(float(ActualIdentity->GetNumberField(TEXT("effective_time_dilation"))), 1.f)) { Finish(TEXT("Target city time dilation differs from one; see identity fields.")); return; }
	if (!ActualIdentity->GetStringField(TEXT("next_url")).IsEmpty()) { Finish(TEXT("Target city has pending travel; see identity fields.")); return; }
	if (!bStarted)
	{
		TArray<FString> Tokens; FString(FCommandLine::Get()).ParseIntoArrayWS(Tokens);
		for (const auto& Token : Tokens) if (Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)
			&& !Token.Equals(TEXT("-RokPowerReplayAudit"), ESearchCase::IgnoreCase)
			&& !Token.StartsWith(TEXT("-RokPowerReplayCaptureTime="), ESearchCase::IgnoreCase)) { Finish(TEXT("Other Rok fixtures/audits forbidden.")); return; }
		if (!FMath::IsFinite(CaptureTime) || CaptureTime < 0 || CaptureTime > 3.5f) { Finish(TEXT("Invalid capture time.")); return; }
		TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, URokMainHudWidget::StaticClass(), false);
		int32 Count = 0;
		for (auto* W : Widgets) if (W->GetWorld() == GetWorld() && W->IsVisible() && W->GetRenderOpacity() > 0 && W->GetCachedGeometry().GetLocalSize().X > 1) { Hud = Cast<URokMainHudWidget>(W); ++Count; }
		if (Count != 1) { Finish(TEXT("Expected unique visible attached HUD.")); return; }
		Effect = Cast<URokPowerGainWidget>(Hud->GetWidgetFromName(TEXT("PlayerPowerGain")));
		auto* Runtime = GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
		if (!Effect.IsValid() || Effect->IsPlaying() || !Runtime) { Finish(TEXT("Expected idle existing HUD power effect/runtime.")); return; }
		Baseline = Runtime->GetPlayerProfileState().LocalPreviewScore;
		if (Baseline < 0 || Baseline > MAX_int32 - 3) { Finish(TEXT("Invalid baseline.")); return; }
		Controller = GetWorld()->GetFirstPlayerController();
		if (!Controller.IsValid()) { Finish(TEXT("Missing player controller.")); return; }
		ViewTarget = Controller->GetViewTarget(); Origin = Elapsed; WallOrigin = FPlatformTime::Seconds(); bStarted = true;
	}
	if (!Hud.IsValid() || !Effect.IsValid() || !Hud->IsVisible() || Hud->GetRenderOpacity() <= 0
		|| !Controller.IsValid() || GetWorld()->GetFirstPlayerController() != Controller.Get() || Controller->GetViewTarget() != ViewTarget.Get()
		|| Hud->GetWidgetFromName(TEXT("PlayerPowerGain")) != Effect.Get()) { Finish(TEXT("HUD/effect/controller identity changed.")); return; }
	auto* Runtime = GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
	if (!Runtime || Runtime->GetPlayerProfileState().LocalPreviewScore != Baseline) { Finish(TEXT("Business score changed during presentation-only audit.")); return; }
	Trace.Add(MakeShared<FJsonValueObject>(State(TEXT("tick"))));
	const double Thresholds[] = { 0, .203, .707 };
	if (Notification < 3 && Elapsed - Origin >= Thresholds[Notification])
	{
		auto Before = State(TEXT("before_notification")); Before->SetNumberField(TEXT("request_threshold"), Thresholds[Notification]); Trace.Add(MakeShared<FJsonValueObject>(Before));
		// Actual public UI adapter. Normal HUD refresh remains enabled and may restore baseline.
		Hud->SetPowerPresentation(Baseline + Notification + 1); ++Notification;
		Trace.Add(MakeShared<FJsonValueObject>(State(TEXT("after_notification"))));
		if (!Effect->IsPlaying()) { Finish(TEXT("UI notification did not start effect.")); return; }
	}
	// One capture per fresh process, like native ReplayVisual: no series of readbacks shifting later targets.
	if (CaptureIndex == 0 && Elapsed - Origin >= CaptureTime && !FScreenshotRequest::IsScreenshotRequested())
	{
		PendingCapture = State(TEXT("screenshot_request")); PendingCapture->SetNumberField(TEXT("request_threshold"), CaptureTime);
		Trace.Add(MakeShared<FJsonValueObject>(PendingCapture));
		PendingPath = Directory / TEXT("actual_city_hud.png"); PendingCapture->SetStringField(TEXT("path"), PendingPath);
		++CaptureIndex; FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
	}
	if (Notification == 3 && !Effect->IsPlaying() && Elapsed - Origin > 3.6 && !PendingCapture.IsValid() && Captures.Num() == 1) Finish(FString());
}
void URokPowerReplayAuditSubsystem::Processed()
{
	if (bFinished || !PendingCapture.IsValid()) return;
	if (IFileManager::Get().FileSize(*PendingPath) <= 0) { Finish(TEXT("Processed screenshot file missing.")); return; }
	auto ProcessedState = State(TEXT("screenshot_processed"));
	Trace.Add(MakeShared<FJsonValueObject>(ProcessedState));
	PendingCapture->SetObjectField(TEXT("processed_state"), ProcessedState);
	Captures.Add(MakeShared<FJsonValueObject>(PendingCapture)); PendingCapture.Reset();
}
void URokPowerReplayAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	bFinished = true;
	auto Root = MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("runtime_complete"), Failure.IsEmpty()); Root->SetStringField(TEXT("failure"), Failure);
	Root->SetObjectField(TEXT("identity"), Identity());
	Root->SetStringField(TEXT("scope"), TEXT("Three explicit local UI presentation notifications via SetPowerPresentation; natural NativeTick only; no business/server update or physical input claim. Normal quarter-second HUD refresh remains enabled. Request thresholds are not actual event times. Offline verifier and visual comparison required; exact equality endpoint remains unit-test evidence."));
	Root->SetNumberField(TEXT("baseline"), Baseline); Root->SetArrayField(TEXT("trace"), Trace); Root->SetArrayField(TEXT("captures"), Captures);
	FString Json; const bool Saved = FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json)) && FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json")));
	UE_LOG(LogTemp, Display, TEXT("PowerReplayAudit complete=%d saved=%d failure=%s directory=%s"), Failure.IsEmpty(), Saved, *Failure, *Directory);
	FPlatformMisc::RequestExitWithStatus(false, Failure.IsEmpty() && Saved ? 0 : 1);
}
