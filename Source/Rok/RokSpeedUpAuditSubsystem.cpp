#include "RokSpeedUpAuditSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/Children.h"
#include "Layout/WidgetPath.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokBuildingPanelWidget.h"
#include "RokMainHudWidget.h"
#include "RokPowerGainWidget.h"
#include "RokUnityFeatureScreenWidget.h"
#include "RokUnityModalWidget.h"
#include "RokUnityRuntimeStateSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"
#include "Widgets/IToolTip.h"
#include "Widgets/Text/STextBlock.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
constexpr int32 SpeedUpAuditCaptureCount = 17;
// This is the same precision projection as the real queue subsystem, never a tolerance or clock advance.
bool SpeedUpAuditQueueReached(double WorldSeconds, float Deadline)
{
	const float QueueSeconds = static_cast<float>(WorldSeconds);
	return FMath::IsFinite(WorldSeconds) && WorldSeconds >= 0 && FMath::IsFinite(QueueSeconds) && QueueSeconds >= Deadline;
}
TSharedRef<FJsonObject> SpeedUpAuditQueueObservation(const FRokBuildingState& State, double WorldSeconds, int32 ActiveQueues)
{
	auto Value = MakeShared<FJsonObject>();
	Value->SetNumberField(TEXT("raw_world_seconds"), WorldSeconds);
	Value->SetNumberField(TEXT("float_queue_seconds"), static_cast<float>(WorldSeconds));
	Value->SetStringField(TEXT("building_key"), State.BuildingKey.ToString());
	Value->SetNumberField(TEXT("init_id"), State.UnityInitId); Value->SetNumberField(TEXT("type_id"), State.UnityBuildingTypeId);
	Value->SetNumberField(TEXT("level"), State.Level); Value->SetNumberField(TEXT("power"), State.Power);
	Value->SetBoolField(TEXT("active"), State.bUpgradeInProgress); Value->SetNumberField(TEXT("active_queue_count"), ActiveQueues);
	Value->SetNumberField(TEXT("started_at"), State.UpgradeStartedAt); Value->SetNumberField(TEXT("finishes_at"), State.UpgradeFinishesAt);
	return Value;
}
bool SpeedUpAuditVisible(const UWidget* Widget)
{
	if (!Widget || !Widget->IsVisible() || Widget->GetRenderOpacity() <= 0) return false;
	for (const UWidget* Parent = Widget->GetParent(); Parent; Parent = Parent->GetParent())
		if (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0) return false;
	for (const UObject* Outer = Widget->GetOuter(); Outer; Outer = Outer->GetOuter())
		if (const auto* Parent = Cast<UUserWidget>(Outer); Parent && (!Parent->IsVisible() || Parent->GetRenderOpacity() <= 0)) return false;
	return true;
}
bool SpeedUpAuditSameResources(const FRokResourceStock& A, const FRokResourceStock& B)
{
	return A.Food == B.Food && A.Wood == B.Wood && A.Stone == B.Stone && A.Gold == B.Gold;
}
TSharedRef<FJsonObject> SpeedUpAuditResources(const FRokResourceStock& Stock)
{
	auto Value = MakeShared<FJsonObject>(); Value->SetNumberField(TEXT("food"), Stock.Food);
	Value->SetNumberField(TEXT("wood"), Stock.Wood); Value->SetNumberField(TEXT("stone"), Stock.Stone); Value->SetNumberField(TEXT("gold"), Stock.Gold); return Value;
}
bool SpeedUpAuditSlateContainsText(const TSharedRef<SWidget>& Widget, const FString& Expected)
{
	if (Widget->GetType() == FName(TEXT("STextBlock"))
		&& StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString() == Expected)
	{
		return true;
	}
	FChildren* Children = Widget->GetAllChildren();
	for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
	{
		if (SpeedUpAuditSlateContainsText(Children->GetChildAt(Index), Expected)) return true;
	}
	return false;
}
bool SpeedUpAuditSlateContainsWidget(const TSharedRef<SWidget>& Widget, const SWidget* Target)
{
	if (&Widget.Get() == Target) return true;
	FChildren* Children = Widget->GetAllChildren();
	for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
	{
		if (SpeedUpAuditSlateContainsWidget(Children->GetChildAt(Index), Target)) return true;
	}
	return false;
}
void SpeedUpAuditInspectTooltipPresenters(const TSharedRef<SWidget>& Widget, const SWidget* Target,
	const FString& Expected, int32& PresenterCount, int32& MatchingTextCount, int32& TargetWidgetCount)
{
	if (Widget->GetType() == FName(TEXT("STooltipPresenter")))
	{
		++PresenterCount;
		if (SpeedUpAuditSlateContainsText(Widget, Expected)) ++MatchingTextCount;
		if (Target && SpeedUpAuditSlateContainsWidget(Widget, Target)) ++TargetWidgetCount;
	}
	FChildren* Children = Widget->GetAllChildren();
	for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
	{
		SpeedUpAuditInspectTooltipPresenters(Children->GetChildAt(Index), Target, Expected,
			PresenterCount, MatchingTextCount, TargetWidgetCount);
	}
}
TSharedRef<FJsonObject> SpeedUpAuditCursorWindowDiagnostics(const TSharedRef<SWindow>& GameWindow,
	const FVector2D& TargetScreenPoint)
{
	auto Result = MakeShared<FJsonObject>();
	const FSlateRect SlateRect = GameWindow->GetRectInScreen();
	Result->SetNumberField(TEXT("target_screen_x"), TargetScreenPoint.X);
	Result->SetNumberField(TEXT("target_screen_y"), TargetScreenPoint.Y);
	Result->SetNumberField(TEXT("slate_window_left"), SlateRect.Left);
	Result->SetNumberField(TEXT("slate_window_top"), SlateRect.Top);
	Result->SetNumberField(TEXT("slate_window_right"), SlateRect.Right);
	Result->SetNumberField(TEXT("slate_window_bottom"), SlateRect.Bottom);
	const TSharedPtr<FGenericWindow> NativeWindow = GameWindow->GetNativeWindow();
	Result->SetBoolField(TEXT("native_window_present"), NativeWindow.IsValid());
#if PLATFORM_WINDOWS
	const HWND GameHandle = NativeWindow.IsValid() ? static_cast<HWND>(NativeWindow->GetOSWindowHandle()) : nullptr;
	Result->SetStringField(TEXT("game_hwnd"), FString::Printf(TEXT("%p"), GameHandle));
	POINT Cursor = {};
	const bool bGotCursor = ::GetCursorPos(&Cursor) != 0;
	Result->SetBoolField(TEXT("os_cursor_read"), bGotCursor);
	if (bGotCursor)
	{
		Result->SetNumberField(TEXT("os_cursor_x"), Cursor.x);
		Result->SetNumberField(TEXT("os_cursor_y"), Cursor.y);
		Result->SetBoolField(TEXT("os_cursor_matches_target"), FMath::Abs(Cursor.x - TargetScreenPoint.X) <= 2.0
			&& FMath::Abs(Cursor.y - TargetScreenPoint.Y) <= 2.0);
	}
	const HWND HitHandle = bGotCursor ? ::WindowFromPoint(Cursor) : nullptr;
	const HWND ForegroundHandle = ::GetForegroundWindow();
	Result->SetStringField(TEXT("window_from_point_hwnd"), FString::Printf(TEXT("%p"), HitHandle));
	Result->SetStringField(TEXT("foreground_hwnd"), FString::Printf(TEXT("%p"), ForegroundHandle));
	Result->SetBoolField(TEXT("window_from_point_is_game"), GameHandle && HitHandle == GameHandle);
	Result->SetBoolField(TEXT("window_from_point_root_is_game"), GameHandle && HitHandle
		&& ::GetAncestor(HitHandle, GA_ROOT) == GameHandle);
	Result->SetBoolField(TEXT("window_from_point_child_of_game"), GameHandle && HitHandle
		&& ::IsChild(GameHandle, HitHandle) != 0);
	Result->SetBoolField(TEXT("foreground_is_game"), GameHandle && ForegroundHandle == GameHandle);
	DWORD HitProcessId = 0, GameProcessId = 0;
	if (HitHandle) ::GetWindowThreadProcessId(HitHandle, &HitProcessId);
	if (GameHandle) ::GetWindowThreadProcessId(GameHandle, &GameProcessId);
	Result->SetBoolField(TEXT("game_hwnd_owned_by_current_process"), GameProcessId == ::GetCurrentProcessId());
	Result->SetBoolField(TEXT("window_from_point_same_process"), HitProcessId != 0 && HitProcessId == GameProcessId);
	Result->SetBoolField(TEXT("game_hwnd_visible"), GameHandle && ::IsWindowVisible(GameHandle) != 0);
	Result->SetBoolField(TEXT("game_hwnd_minimized"), GameHandle && ::IsIconic(GameHandle) != 0);
	RECT WindowRect = {};
	if (GameHandle && ::GetWindowRect(GameHandle, &WindowRect))
	{
		Result->SetNumberField(TEXT("native_window_left"), WindowRect.left);
		Result->SetNumberField(TEXT("native_window_top"), WindowRect.top);
		Result->SetNumberField(TEXT("native_window_right"), WindowRect.right);
		Result->SetNumberField(TEXT("native_window_bottom"), WindowRect.bottom);
	}
	RECT ClientRect = {};
	if (GameHandle && ::GetClientRect(GameHandle, &ClientRect))
	{
		POINT ClientMin = {ClientRect.left, ClientRect.top};
		POINT ClientMax = {ClientRect.right, ClientRect.bottom};
		if (::ClientToScreen(GameHandle, &ClientMin) && ::ClientToScreen(GameHandle, &ClientMax))
		{
			Result->SetNumberField(TEXT("native_client_left"), ClientMin.x);
			Result->SetNumberField(TEXT("native_client_top"), ClientMin.y);
			Result->SetNumberField(TEXT("native_client_right"), ClientMax.x);
			Result->SetNumberField(TEXT("native_client_bottom"), ClientMax.y);
			Result->SetBoolField(TEXT("os_cursor_inside_game_client"), bGotCursor
				&& Cursor.x >= ClientMin.x && Cursor.x < ClientMax.x
				&& Cursor.y >= ClientMin.y && Cursor.y < ClientMax.y);
		}
	}
#else
	Result->SetStringField(TEXT("os_window_diagnostics"), TEXT("Windows-only audit"));
#endif
	return Result;
}
TSharedRef<FJsonObject> SpeedUpAuditActivateVerifiedGameWindow(const TSharedRef<SWindow>& GameWindow,
	const FVector2D& TargetScreenPoint)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("attempted"), false);
#if PLATFORM_WINDOWS
	const TSharedPtr<FGenericWindow> NativeWindow = GameWindow->GetNativeWindow();
	const HWND GameHandle = NativeWindow.IsValid() ? static_cast<HWND>(NativeWindow->GetOSWindowHandle()) : nullptr;
	DWORD OwnerProcessId = 0;
	if (GameHandle) ::GetWindowThreadProcessId(GameHandle, &OwnerProcessId);
	const bool bOwnGameWindow = FParse::Param(FCommandLine::Get(), TEXT("game"))
		&& GameHandle && ::IsWindow(GameHandle) != 0 && OwnerProcessId == ::GetCurrentProcessId();
	Result->SetBoolField(TEXT("verified_own_game_window"), bOwnGameWindow);
	if (!bOwnGameWindow) return Result;
	RECT ClientRect = {};
	POINT ClientMin = {}, ClientMax = {};
	const bool bClientRectValid = ::GetClientRect(GameHandle, &ClientRect) != 0;
	if (bClientRectValid)
	{
		ClientMin = {ClientRect.left, ClientRect.top};
		ClientMax = {ClientRect.right, ClientRect.bottom};
	}
	const bool bClientScreenValid = bClientRectValid
		&& ::ClientToScreen(GameHandle, &ClientMin) != 0 && ::ClientToScreen(GameHandle, &ClientMax) != 0;
	const bool bTargetInsideClient = bClientScreenValid
		&& TargetScreenPoint.X >= ClientMin.x && TargetScreenPoint.X < ClientMax.x
		&& TargetScreenPoint.Y >= ClientMin.y && TargetScreenPoint.Y < ClientMax.y;
	Result->SetBoolField(TEXT("target_inside_game_client"), bTargetInsideClient);
	if (!bTargetInsideClient) return Result;
	Result->SetBoolField(TEXT("attempted"), true);
	const bool bWasVisible = ::IsWindowVisible(GameHandle) != 0;
	const bool bWasMinimized = ::IsIconic(GameHandle) != 0;
	Result->SetBoolField(TEXT("visible_before"), bWasVisible);
	Result->SetBoolField(TEXT("minimized_before"), bWasMinimized);
	if (bWasMinimized) ::ShowWindow(GameHandle, SW_RESTORE);
	else if (!bWasVisible) ::ShowWindow(GameHandle, SW_SHOW);
	Result->SetBoolField(TEXT("visible_after_show"), ::IsWindowVisible(GameHandle) != 0);
	Result->SetBoolField(TEXT("minimized_after_show"), ::IsIconic(GameHandle) != 0);
	Result->SetBoolField(TEXT("set_foreground_returned"), ::SetForegroundWindow(GameHandle) != 0);
	Result->SetBoolField(TEXT("foreground_is_game_after_call"), ::GetForegroundWindow() == GameHandle);
#else
	Result->SetStringField(TEXT("unsupported_platform"), TEXT("Windows-only GameApp hover audit"));
#endif
	return Result;
}
}

bool URokSpeedUpAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return FParse::Param(FCommandLine::Get(), TEXT("RokSpeedUpAudit")) && !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
#endif
}
bool URokSpeedUpAuditSubsystem::DoesSupportWorldType(EWorldType::Type Type) const { return Type == EWorldType::Game; }
void URokSpeedUpAuditSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection); StartedWall = FPlatformTime::Seconds();
	bTaskHoverAuditRequested = FParse::Param(FCommandLine::Get(), TEXT("RokTaskProgressHoverAudit"));
	Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("RokUnityMigration/SpeedUpAudit")
		/ FString::Printf(TEXT("%s_%u"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FPlatformProcess::GetCurrentProcessId()));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ScreenshotHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(this, &URokSpeedUpAuditSubsystem::ScreenshotProcessed);
}
void URokSpeedUpAuditSubsystem::Deinitialize()
{
	RestoreTaskHoverPointer();
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotHandle);
	if (Upgrades.IsValid()) Upgrades->OnLocalUpgradeCompleted.Remove(CompletionHandle);
	if (!bFinished) { Failures.Add(TEXT("World ended before natural queue completion audit finished.")); Save(false); }
	Super::Deinitialize();
}
TStatId URokSpeedUpAuditSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(URokSpeedUpAuditSubsystem, STATGROUP_Tickables); }

bool URokSpeedUpAuditSubsystem::CheckIdentity()
{
	FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()); FPaths::NormalizeDirectoryName(Project);
	if (!FPaths::IsSamePath(Project, TEXT("D:/ueDemo/Rok")) || !GetWorld()
		|| GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Maps/L_RokUnityGameAppCity"))
	{ Finish(TEXT("Wrong project/map; requires the actual Rok city.")); return false; }
	if (GetWorld()->IsPaused() || !FMath::IsNearlyEqual(GetWorld()->GetWorldSettings()->GetEffectiveTimeDilation(), 1.f)
		|| !GetWorld()->NextURL.IsEmpty())
	{ Finish(TEXT("Pause, time dilation or travel invalidated the natural timer audit.")); return false; }
	if (bBegun && (!Controller.IsValid() || GetWorld()->GetFirstPlayerController() != Controller.Get()
		|| Controller->GetPawn() != InitialPawn.Get() || Controller->GetViewTarget() != InitialViewTarget.Get()))
	{ Finish(TEXT("Controller, pawn or camera view target changed during queue audit.")); return false; }
	return true;
}

bool URokSpeedUpAuditSubsystem::CheckSelectedActor(UUserWidget* Widget, AActor* Expected)
{
	UObject* Actual = nullptr;
	bool bReadable = false;
	const TCHAR* Method = TEXT("reflected SelectedBuildingActor");
	if (const auto* BuildingModal = Cast<URokUnityModalWidget>(Widget))
	{
		// Modal's weak actor is deliberately not a UPROPERTY. Panel's field is reflected.
		Method = TEXT("modal read-only actor getter");
#if !UE_BUILD_SHIPPING
		Actual = BuildingModal->GetSelectedBuildingActorForAudit();
		bReadable = true;
#endif
	}
	else if (const auto* Property = Widget ? FindFProperty<FObjectPropertyBase>(Widget->GetClass(), TEXT("SelectedBuildingActor")) : nullptr)
	{
		Actual = Property->GetObjectPropertyValue_InContainer(Widget);
		bReadable = true;
	}
	if (!bReadable || !Expected || Actual != Expected)
	{
		Finish(FString::Printf(TEXT("Selected-building identity mismatch: widget=%s class=%s reader=%s readable=%d expected=%s actual=%s."),
			*GetNameSafe(Widget), Widget ? *Widget->GetClass()->GetName() : TEXT("<null>"), Method, bReadable,
			*GetNameSafe(Expected), *GetNameSafe(Actual)));
		return false;
	}
	return true;
}

bool URokSpeedUpAuditSubsystem::Begin()
{
	if (!CheckIdentity()) return false;
	// Initial selection is the existing controller fixture. Every subsequent window uses its real UMG button.
	const TCHAR* Cursor = FCommandLine::Get(); FString Token; int32 SelectFlags = 0;
	while (FParse::Token(Cursor, Token, false))
	{
		if (!Token.StartsWith(TEXT("-Rok"), ESearchCase::IgnoreCase)) continue;
		if (Token.Equals(TEXT("-RokSpeedUpAudit"), ESearchCase::IgnoreCase)) continue;
		if (bTaskHoverAuditRequested && Token.Equals(TEXT("-RokTaskProgressHoverAudit"), ESearchCase::IgnoreCase)) continue;
		if (Token.Equals(TEXT("-RokAutoSelectBuilding=UnityInitId_6"), ESearchCase::IgnoreCase)) { ++SelectFlags; continue; }
		Finish(TEXT("Run SpeedUpAudit only with its exact AutoSelectBuilding fixture; other Rok flags are forbidden.")); return false;
	}
	if (SelectFlags != 1) { Finish(TEXT("Requires exactly -RokAutoSelectBuilding=UnityInitId_6.")); return false; }
	int32 TargetCount = 0, WorkerCount = 0, HallCount = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("UnityInitId_5"))) { HallActor = *It; ++HallCount; }
		if (It->ActorHasTag(TEXT("UnityInitId_6"))) { TargetActor = *It; ++TargetCount; }
		if (It->ActorHasTag(TEXT("UnityInitId_2"))) { WorkerActor = *It; ++WorkerCount; }
	}
	if (TargetCount != 1 || WorkerCount != 1 || HallCount != 1 || !HallActor->ActorHasTag(TEXT("UnityBuildingType_1")) || !TargetActor->ActorHasTag(TEXT("UnityBuildingType_9"))
		|| !WorkerActor->ActorHasTag(TEXT("UnityBuildingType_7")))
	{ Finish(TEXT("Expected unique actual Init5 city hall, Init6 barracks and Init2 worker hut actors.")); return false; }
	TArray<UUserWidget*> Found; int32 Count = 0;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, URokMainHudWidget::StaticClass(), false);
	for (auto* Widget : Found) if (Widget->GetWorld() == GetWorld() && SpeedUpAuditVisible(Widget) && Widget->GetCachedGeometry().GetLocalSize().X > 1)
	{ Hud = Cast<URokMainHudWidget>(Widget); ++Count; }
	if (Count != 1) { Finish(TEXT("Expected one attached visible city HUD.")); return false; }
	Panel = Cast<URokBuildingPanelWidget>(Hud->GetWidgetFromName(TEXT("BuildingPanelWidget")));
	Modal = Cast<URokUnityModalWidget>(Hud->GetWidgetFromName(TEXT("UnityModalWidget")));
	Feature = Cast<URokUnityFeatureScreenWidget>(Hud->GetWidgetFromName(TEXT("UnityFeatureScreenWidget")));
	Upgrades = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
	Controller = GetWorld()->GetFirstPlayerController();
	if (!Panel.IsValid() || !Modal.IsValid() || !Feature.IsValid() || !Upgrades.IsValid() || !Controller.IsValid()
		|| !Controller->GetPawn() || !Controller->GetViewTarget() || Modal->IsModalOpen() || Feature->IsScreenOpen())
	{ Finish(TEXT("Expected closed modal/feature hosts and a real city controller before interaction.")); return false; }
	InitialPawn = Controller->GetPawn(); InitialViewTarget = Controller->GetViewTarget();
	RuntimeState = GetWorld()->GetGameInstance()->GetSubsystem<URokUnityRuntimeStateSubsystem>();
	PowerGain = Cast<URokPowerGainWidget>(Hud->GetWidgetFromName(TEXT("PlayerPowerGain")));
	if (!RuntimeState.IsValid() || !RuntimeState->IsRuntimeStateLoaded() || !PowerGain.IsValid()
		|| PowerGain->IsPlaying() || Upgrades->GetLocalCompletedBuildingPowerDelta() != 0)
	{ Finish(TEXT("Expected loaded immutable profile baseline and no previous local power gain.")); return false; }
	const auto Profile = RuntimeState->GetPlayerProfileState(); InitialScore = Profile.LocalPreviewScore;
	if (Profile.LocalBuildingUpgradeScore != 0 || InitialScore <= 0)
	{ Finish(TEXT("Expected the positive bootstrap preview score with zero local building increment.")); return false; }
	CompletionHandle = Upgrades->OnLocalUpgradeCompleted.AddUObject(this, &URokSpeedUpAuditSubsystem::UpgradeCompleted);
	InitialState = Upgrades->GetBuildingStateForActor(TargetActor.Get());
	HallInitialState = Upgrades->GetBuildingStateForActor(HallActor.Get());
	const auto WorkerState = Upgrades->GetBuildingStateForActor(WorkerActor.Get());
	FRokBuildingUpgradePreview Preview;
	if (InitialState.UnityInitId != 6 || InitialState.UnityBuildingTypeId != 9 || InitialState.Level != 1
		|| InitialState.bUpgradeInProgress || WorkerState.UnityInitId != 2 || WorkerState.UnityBuildingTypeId != 7
		|| Upgrades->GetActiveUpgradeCount() != 0 || !Upgrades->GetUpgradePreview(TargetActor.Get(), Preview)
		|| Preview.bCanStartTimedUpgrade || Preview.FailureReason != TEXT("需要 2 级市政厅（当前 1 级）") || Preview.TargetLevel != 2 || Preview.BuildSeconds != 20
		|| Preview.Cost.Food != 500 || Preview.Cost.Wood != 800 || Preview.Cost.Stone != 0 || Preview.Cost.Gold != 0 || Preview.NextPower != 10)
	{ Finish(TEXT("Initial barracks must be blocked by source row902 requiring completed CityHall2.")); return false; }
	FRokBuildingUpgradePreview HallPreview;
	if (HallInitialState.UnityInitId != 5 || HallInitialState.UnityBuildingTypeId != 1 || HallInitialState.Level != 1
		|| HallInitialState.Power != 7 || HallInitialState.bUpgradeInProgress || !Upgrades->GetUpgradePreview(HallActor.Get(), HallPreview)
		|| !HallPreview.bCanStartTimedUpgrade || HallPreview.TargetLevel != 2 || HallPreview.BuildSeconds != 2
		|| HallPreview.Cost.Food != 3500 || HallPreview.Cost.Wood != 3500 || HallPreview.Cost.Stone != 0 || HallPreview.Cost.Gold != 0 || HallPreview.NextPower != 21)
	{ Finish(TEXT("Existing source Init5 cannot perform the exact legal row102 prerequisite queue.")); return false; }
	ResourcesInitial = Upgrades->GetCurrentResources();
	if (ResourcesInitial.Food != 5000 || ResourcesInitial.Wood != 5000 || ResourcesInitial.Stone != 1500 || ResourcesInitial.Gold != 600)
	{ Finish(TEXT("Expected untouched explicit local bootstrap resources before the legal prerequisite chain.")); return false; }
	bBegun = true;
	return CheckSelectedActor(Panel.Get(), TargetActor.Get()) && CheckPower() && Validate();
}

bool URokSpeedUpAuditSubsystem::BindHallQueue()
{
	if (!HallActor.IsValid() || !Upgrades.IsValid()) { Finish(TEXT("Prerequisite source actor vanished after timed upgrade.")); return false; }
	HallBoundState = Upgrades->GetBuildingStateForActor(HallActor.Get());
	ResourcesAfterHall = Upgrades->GetCurrentResources();
	FRokResourceStock Expected = ResourcesInitial; Expected.Food -= 3500; Expected.Wood -= 3500;
	if (!bHallStartRequested || HallBoundState.BuildingKey != HallInitialState.BuildingKey || HallBoundState.UnityInitId != 5
		|| HallBoundState.UnityBuildingTypeId != 1 || HallBoundState.Level != 1 || HallBoundState.Power != 7 || !HallBoundState.bUpgradeInProgress
		|| !FMath::IsFinite(HallBoundState.UpgradeStartedAt) || !FMath::IsFinite(HallBoundState.UpgradeFinishesAt)
		|| !FMath::IsNearlyEqual(HallBoundState.UpgradeFinishesAt - HallBoundState.UpgradeStartedAt, 2.f, .001f)
		|| Upgrades->GetActiveUpgradeCount() != 1 || !SpeedUpAuditSameResources(ResourcesAfterHall, Expected))
	{ Finish(TEXT("CityHall TimedUpgradeButton did not start row102 with one exact resource debit.")); return false; }
	bHallBound = true; return CheckHallQueue();
}

bool URokSpeedUpAuditSubsystem::CheckHallQueue()
{
	if (!bBegun) return true;
	if (!HallActor.IsValid() || !TargetActor.IsValid() || !WorkerActor.IsValid() || !Upgrades.IsValid())
	{ Finish(TEXT("An actual source actor was lost; cannot count prerequisite completion.")); return false; }
	const auto Hall = Upgrades->GetBuildingStateForActor(HallActor.Get());
	LastHallObservation = SpeedUpAuditQueueObservation(Hall, GetWorld()->GetTimeSeconds(), Upgrades->GetActiveUpgradeCount());
	if (Hall.BuildingKey != HallInitialState.BuildingKey || Hall.UnityInitId != 5 || Hall.UnityBuildingTypeId != 1)
	{ Finish(TEXT("Prerequisite building source identity changed.")); return false; }
	if (!bHallBound)
	{
		if (!bHallStartRequested && (Hall.bUpgradeInProgress || Hall.Level != 1 || Hall.Power != 7
			|| Upgrades->GetActiveUpgradeCount() != 0 || !SpeedUpAuditSameResources(ResourcesInitial, Upgrades->GetCurrentResources())))
		{ Finish(TEXT("Blocked preview changed resources, queue or prerequisite level before a permitted click.")); return false; }
		return true;
	}
	if (Hall.bUpgradeInProgress)
	{
		if (bHallCompleted || Hall.Level != 1 || Hall.Power != 7 || Hall.UpgradeStartedAt != HallBoundState.UpgradeStartedAt
			|| Hall.UpgradeFinishesAt != HallBoundState.UpgradeFinishesAt || Upgrades->GetActiveUpgradeCount() != 1)
		{ Finish(TEXT("Prerequisite queue was replaced, accelerated or restarted.")); return false; }
	}
	else
	{
		if (!SpeedUpAuditQueueReached(GetWorld()->GetTimeSeconds(), HallBoundState.UpgradeFinishesAt) || Hall.Level != 2 || Hall.Power != 21
			|| Hall.UpgradeStartedAt != 0 || Hall.UpgradeFinishesAt != 0)
		{ Finish(TEXT("Prerequisite queue disappeared without exact source natural completion.")); return false; }
		if (!bHallCompleted)
		{
			HallCompletionWall = FPlatformTime::Seconds();
			if (HallCompletionWall - HallClickedWall < 1.9)
			{ Finish(TEXT("Source two-second prerequisite did not elapse in real wall time.")); return false; }
			bHallCompleted = true;
		}
	}
	if (!bTargetStartRequested)
	{
		const auto Barracks = Upgrades->GetBuildingStateForActor(TargetActor.Get());
		FRokBuildingUpgradePreview Preview;
		if (!SpeedUpAuditSameResources(ResourcesAfterHall, Upgrades->GetCurrentResources())
			|| Barracks.BuildingKey != InitialState.BuildingKey || Barracks.Level != 1 || Barracks.bUpgradeInProgress
			|| Upgrades->GetActiveUpgradeCount() != (bHallCompleted ? 0 : 1)
			|| !Upgrades->GetUpgradePreview(TargetActor.Get(), Preview) || Preview.bCanStartTimedUpgrade != bHallCompleted
			|| (!bHallCompleted && Preview.FailureReason != TEXT("需要 2 级市政厅（当前 1 级）")))
		{ Finish(TEXT("Barracks must unlock only after the prerequisite commits, without additional resource mutation.")); return false; }
	}
	return true;
}

bool URokSpeedUpAuditSubsystem::SelectExistingBuilding(AActor* Actor)
{
	if (!Actor || (Actor != HallActor.Get() && Actor != TargetActor.Get() && Actor != WorkerActor.Get()) || !Validate()) return false;
	auto Action = MakeShared<FJsonObject>();
	Action->SetStringField(TEXT("selection_fixture"), TEXT("Hud.SetSelectedBuilding(existing verified source actor); not a physical world hit-test"));
	Action->SetStringField(TEXT("actor"), Actor->GetName()); Action->SetNumberField(TEXT("frame"), double(Frame));
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	Hud->SetSelectedBuilding(Actor); bHallSelected = Actor == HallActor.Get(); bWorkerSelected = Actor == WorkerActor.Get();
	return true;
}

bool URokSpeedUpAuditSubsystem::BindActualQueue()
{
	if (!bTargetBaselineRecorded || !bHallCompleted || CompletionEvents != 1 || !Upgrades.IsValid() || !TargetActor.IsValid()) { Finish(TEXT("Legal prerequisite baseline vanished before barracks queue binding.")); return false; }
	BoundState = Upgrades->GetBuildingStateForActor(TargetActor.Get());
	ResourcesAfter = Upgrades->GetCurrentResources();
	FRokResourceStock Expected = ResourcesBefore; Expected.Food -= 500; Expected.Wood -= 800;
	if (!BoundState.bUpgradeInProgress || BoundState.BuildingKey != InitialState.BuildingKey || BoundState.UnityInitId != 6
		|| BoundState.Level != 1 || BoundState.UnityBuildingTypeId != 9
		|| !FMath::IsFinite(BoundState.UpgradeStartedAt) || !FMath::IsFinite(BoundState.UpgradeFinishesAt)
		|| !FMath::IsNearlyEqual(BoundState.UpgradeFinishesAt - BoundState.UpgradeStartedAt, 20.f, .001f)
		|| Upgrades->GetActiveUpgradeCount() != 1 || !SpeedUpAuditSameResources(ResourcesAfter, Expected))
	{ Finish(TEXT("TimedUpgradeButton did not create exactly the source-duration local queue and one configured resource debit.")); return false; }
	bBound = true; return CheckQueue();
}

bool URokSpeedUpAuditSubsystem::CheckQueue()
{
	if (!bBound) return true;
	if (!TargetActor.IsValid() || !Upgrades.IsValid()) { Finish(TEXT("Bound queue actor/subsystem lost; cannot report completion.")); return false; }
	const auto Current = Upgrades->GetBuildingStateForActor(TargetActor.Get());
	const double Now = GetWorld()->GetTimeSeconds();
	LastQueueObservation = SpeedUpAuditQueueObservation(Current, Now, Upgrades->GetActiveUpgradeCount());
	if (Current.BuildingKey != BoundState.BuildingKey || Current.UnityInitId != 6 || Current.UnityBuildingTypeId != 9
		|| !SpeedUpAuditSameResources(ResourcesAfter, Upgrades->GetCurrentResources()))
	{ Finish(TEXT("Queue identity or resources changed during directory browsing.")); return false; }
	if (Current.bUpgradeInProgress)
	{
		if (bCompleted || Current.Level != 1 || Current.UpgradeStartedAt != BoundState.UpgradeStartedAt
			|| Current.UpgradeFinishesAt != BoundState.UpgradeFinishesAt || Upgrades->GetActiveUpgradeCount() != 1)
		{ Finish(TEXT("Bound queue was replaced, accelerated or restarted.")); return false; }
	}
	else
	{
		if (!SpeedUpAuditQueueReached(Now, BoundState.UpgradeFinishesAt) || Current.Level != 2 || Current.Power != 10
			|| Current.UpgradeStartedAt != 0 || Current.UpgradeFinishesAt != 0 || Upgrades->GetActiveUpgradeCount() != 0)
		{ Finish(TEXT("Queue disappeared without the single expected natural completion.")); return false; }
		if (!bCompleted)
		{
			CompletionWall = FPlatformTime::Seconds();
			if (CompletionWall - UpgradeClickedWall < 19.5)
			{ Finish(TEXT("Source 20-second queue completed without sufficient real wall time.")); return false; }
			bCompleted = true; ++CompletionTransitions;
		}
	}
	if (Now - LastSampleTime >= .5 || (bCompleted && CompletionTransitions == 1 && LastSampleTime < BoundState.UpgradeFinishesAt))
	{
		auto Sample = MakeShared<FJsonObject>(); Sample->SetNumberField(TEXT("world_seconds"), Now);
		Sample->SetNumberField(TEXT("wall_seconds_since_start_button"), FPlatformTime::Seconds() - UpgradeClickedWall);
		Sample->SetNumberField(TEXT("level"), Current.Level); Sample->SetBoolField(TEXT("active"), Current.bUpgradeInProgress);
		Sample->SetNumberField(TEXT("started_at"), Current.UpgradeStartedAt); Sample->SetNumberField(TEXT("finishes_at"), Current.UpgradeFinishesAt);
		QueueSamples.Add(MakeShared<FJsonValueObject>(Sample)); LastSampleTime = Now;
	}
	return true;
}

void URokSpeedUpAuditSubsystem::UpgradeCompleted(const FRokLocalBuildingUpgradeCompleted& Result)
{
	if (bFinished) return;
	++CompletionEvents;
	const bool bHallEvent = Result.UnityInitId == 5;
	const auto& Expected = bHallEvent ? HallBoundState : BoundState;
	if (!(bHallEvent ? bHallBound : bBound) || CompletionEvents != (bHallEvent ? 1 : 2)
		|| (!bHallEvent && (!bHallCompleted || !bTargetBaselineRecorded)) || Result.BuildingKey != Expected.BuildingKey
		|| Result.UnityInitId != (bHallEvent ? 5 : 6) || Result.UnityBuildingTypeId != (bHallEvent ? 1 : 9)
		|| Result.PreviousLevel != 1 || Result.CurrentLevel != 2 || Result.PreviousPower != (bHallEvent ? 7 : 5)
		|| Result.CurrentPower != (bHallEvent ? 21 : 10) || !SpeedUpAuditQueueReached(GetWorld()->GetTimeSeconds(), Expected.UpgradeFinishesAt))
	{ Finish(TEXT("Local completion event is duplicate, early or differs from legal source 101->102 then 901->902.")); return; }
	auto Event = MakeShared<FJsonObject>(); Event->SetStringField(TEXT("event"), TEXT("OnLocalUpgradeCompleted"));
	Event->SetNumberField(TEXT("init_id"), Result.UnityInitId); Event->SetNumberField(TEXT("sequence"), CompletionEvents);
	Event->SetNumberField(TEXT("old_level"), Result.PreviousLevel); Event->SetNumberField(TEXT("new_level"), Result.CurrentLevel);
	Event->SetNumberField(TEXT("old_power"), Result.PreviousPower); Event->SetNumberField(TEXT("new_power"), Result.CurrentPower);
	Event->SetNumberField(TEXT("world_seconds"), GetWorld()->GetTimeSeconds()); Actions.Add(MakeShared<FJsonValueObject>(Event));
}

bool URokSpeedUpAuditSubsystem::CheckPower()
{
	if (!bBegun) return true;
	if (!RuntimeState.IsValid() || !PowerGain.IsValid() || !Hud.IsValid() || !Upgrades.IsValid())
	{ Finish(TEXT("Power projection/presentation ownership disappeared.")); return false; }
	const auto Profile = RuntimeState->GetPlayerProfileState();
	const int32 Delta = (bHallCompleted ? 14 : 0) + (bCompleted ? 5 : 0);
	const int32 CompletedCount = int32(bHallCompleted) + int32(bCompleted);
	if (Upgrades->GetLocalCompletedBuildingPowerDelta() != Delta || Profile.LocalBuildingUpgradeScore != Delta
		|| Profile.LocalPreviewScore != InitialScore + Delta || CompletionEvents > CompletedCount
		|| (bTargetBaselineRecorded && BaselineScore != InitialScore + 14))
	{ Finish(TEXT("Local power must follow actual completion: baseline, +14 prerequisite, then cumulative +19.")); return false; }
	const bool bPlaying = PowerGain->IsPlaying(); const float Playback = PowerGain->GetPlaybackTime();
	if (bPlaying && (CompletedCount == 0 || (bTargetBaselineRecorded && !bCompleted) || !SpeedUpAuditVisible(PowerGain.Get())))
	{ Finish(TEXT("Power gain played before natural completion or under a hidden HUD.")); return false; }
	if (bPlaying && !bPowerWasPlaying) ++PowerPlayCount;
	if (PowerPlayCount > CompletedCount || (bPlaying && bPowerWasPlaying && Playback + .0001f < LastPowerPlayback))
	{ Finish(TEXT("Power gain restarted or replayed beyond the two actual source queue completions.")); return false; }
	if (bPlaying || bPowerWasPlaying)
	{
		auto Sample = MakeShared<FJsonObject>(); Sample->SetNumberField(TEXT("frame"), double(Frame));
		Sample->SetNumberField(TEXT("world_seconds"), GetWorld()->GetTimeSeconds()); Sample->SetNumberField(TEXT("playback_seconds"), Playback);
		Sample->SetNumberField(TEXT("remaining_seconds"), PowerGain->GetRemainingTime()); Sample->SetBoolField(TEXT("playing"), bPlaying);
		Sample->SetNumberField(TEXT("local_score"), Profile.LocalPreviewScore); PowerSamples.Add(MakeShared<FJsonValueObject>(Sample));
	}
	bPowerWasPlaying = bPlaying; LastPowerPlayback = Playback;
	return true;
}

bool URokSpeedUpAuditSubsystem::IsMidAnimation(bool bOpening) const
{
	const UWidget* Root = Feature.IsValid() ? Feature->GetWidgetFromName(TEXT("FeatureModeCanvas")) : nullptr;
	if (!Feature.IsValid() || !Feature->IsScreenOpen() || !Root) return false;
	// Shared Type1 Open animates scale only; Close animates both scale and alpha.
	return bOpening ? Root->GetRenderOpacity() == 1 && Root->GetRenderTransform().Scale.X > 1.051f
		&& Root->GetRenderTransform().Scale.X < 1.101f : Root->GetRenderOpacity() > .02f && Root->GetRenderOpacity() < .98f;
}

bool URokSpeedUpAuditSubsystem::Validate()
{
	if (!CheckIdentity() || !CheckHallQueue() || !CheckQueue() || !CheckPower()) return false;
	if (!Hud.IsValid() || !Panel.IsValid() || !Modal.IsValid() || !Feature.IsValid() || !SpeedUpAuditVisible(Hud.Get()))
	{ Finish(TEXT("Runtime widget ownership disappeared.")); return false; }
	if (View == EView::City || View == EView::HallActive || View == EView::HallComplete || View == EView::CompletedCity || View == EView::PowerEarly || View == EView::PowerLater)
	{
		if (Modal->IsModalOpen() || Feature->IsScreenOpen() || SpeedUpAuditVisible(Modal.Get()) || SpeedUpAuditVisible(Feature.Get())
			|| !CheckSelectedActor(Panel.Get(), bWorkerSelected ? WorkerActor.Get() : (bHallSelected ? HallActor.Get() : TargetActor.Get())))
		{ if (!bFinished) Finish(TEXT("Expected city with both windows closed.")); return false; }
		if (View == EView::HallActive && (!bHallBound || bHallCompleted || CompletionEvents != 0))
		{ Finish(TEXT("Expected actual in-progress CityHall prerequisite capture.")); return false; }
		if (View == EView::HallComplete)
		{
			const auto* PowerText = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("PlayerPowerText")));
			if (!bHallCompleted || CompletionEvents != 1 || PowerPlayCount != 1 || PowerGain->IsPlaying()
				|| !SpeedUpAuditVisible(PowerText) || PowerText->GetText().ToString() != FText::AsNumber(InitialScore + 14).ToString())
			{ Finish(TEXT("Prerequisite must settle at source +14 before recording the barracks baseline.")); return false; }
		}
		if (View == EView::CompletedCity && (!bCompleted || CompletionTransitions != 1))
		{ Finish(TEXT("City was restored before a single natural completion.")); return false; }
		if (View == EView::PowerEarly || View == EView::PowerLater)
		{
			const float Time = PowerGain->GetPlaybackTime();
			if (!bCompleted || CompletionEvents != 2 || PowerPlayCount != 2 || !PowerGain->IsPlaying()
				|| Time < (View == EView::PowerEarly ? .08f : .45f) || Time > (View == EView::PowerEarly ? .5f : .9f))
			{ Finish(TEXT("Missed the actual early/later power-gain playback after natural completion.")); return false; }
		}
		if (View == EView::CompletedCity)
		{
			const auto* PowerText = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("PlayerPowerText")));
			if (CompletionEvents != 2 || PowerPlayCount != 2 || PowerGain->IsPlaying() || !SpeedUpAuditVisible(PowerText)
				|| PowerText->GetText().ToString() != FText::AsNumber(BaselineScore + 5).ToString())
			{ Finish(TEXT("Final HUD power did not settle at baseline + 5 after one event/effect.")); return false; }
		}
		return true;
	}
	if (View == EView::Upgrade || View == EView::BlockedUpgrade || View == EView::HallUpgrade)
	{
		const auto* Timed = Cast<UButton>(Modal->GetWidgetFromName(TEXT("TimedUpgradeButton")));
		const auto* Close = Cast<UButton>(Modal->GetWidgetFromName(TEXT("Type2CloseButton")));
		if (!Modal->IsModalOpen() || Feature->IsScreenOpen() || !SpeedUpAuditVisible(Timed)
			|| !SpeedUpAuditVisible(Close) || !Close->GetIsEnabled()
			|| !CheckSelectedActor(Modal.Get(), View == EView::HallUpgrade ? HallActor.Get() : TargetActor.Get())
			|| Timed->GetIsEnabled() != (View != EView::BlockedUpgrade))
		{ if (!bFinished) Finish(TEXT("Actual upgrade dialog identity/enabled state differs from source prerequisites.")); return false; }
		if (View == EView::BlockedUpgrade)
		{
			const auto* Feedback = Cast<UTextBlock>(Modal->GetWidgetFromName(TEXT("UpgradeFeedback")));
			if (!SpeedUpAuditVisible(Feedback) || Feedback->GetText().ToString() != TEXT("需要 2 级市政厅（当前 1 级）"))
			{ Finish(TEXT("Blocked upgrade does not display the exact localized unmet source requirement.")); return false; }
		}
		return true;
	}
	if (Modal->IsModalOpen() || !Feature->IsScreenOpen() || !SpeedUpAuditVisible(Feature.Get()))
	{ Finish(TEXT("Expected the active feature window.")); return false; }
	if (View == EView::Worker || View == EView::WorkerClosing)
	{
		const auto* Title = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("WorkerQueuePrimaryTitle")));
		const auto* Progress = Cast<UProgressBar>(Feature->GetWidgetFromName(TEXT("WorkerQueuePrimaryProgress")));
		const auto* Action = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("WorkerQueuePrimaryActionLabel")));
		if (!bCompleted || CompletionTransitions != 1 || !Title || Title->GetText().ToString() != TEXT("您的建造队列正处于空闲状态")
			|| !Progress || Progress->GetPercent() != 0 || !Action || Action->GetText().ToString() != TEXT("建造")
			|| !SpeedUpAuditVisible(Feature->GetWidgetFromName(TEXT("WorkerQueuePrimaryAction"))))
		{ Finish(TEXT("Worker queue did not return to the actual idle/build state.")); return false; }
		return true;
	}
#if !UE_BUILD_SHIPPING
	if (Feature->GetFeatureContextActorForAudit() != TargetActor.Get())
	{ Finish(TEXT("SpeedUp route is not bound to the actual selected building.")); return false; }
#endif
	const auto* Progress = Cast<UProgressBar>(Feature->GetWidgetFromName(TEXT("SpeedUpQueueProgress")));
	const auto* Description = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("SpeedUpQueueDescription")));
	if (!Progress || !Description || !SpeedUpAuditVisible(Progress) || !SpeedUpAuditVisible(Description))
	{ Finish(TEXT("SpeedUp countdown widgets missing or hidden.")); return false; }
	if (View == EView::NaturalClosing)
	{
		if (!bCompleted || CompletionTransitions != 1 || !IsMidAnimation() || Progress->GetPercent() != 1)
		{ Finish(TEXT("Natural completion did not animate close while retaining completed progress.")); return false; }
		return true;
	}
	if (bCompleted) { Finish(TEXT("Queue finished before the requested browse/reopen state was captured.")); return false; }
	const float Expected = FMath::Clamp((GetWorld()->GetTimeSeconds() - BoundState.UpgradeStartedAt) / 20.f, 0.f, 1.f);
	if (!FMath::IsNearlyEqual(Progress->GetPercent(), Expected, .04f)
		|| !Description->GetText().ToString().Contains(TEXT("剩余")))
	{ Finish(TEXT("Presented countdown/progress differs from the bound natural queue clock.")); return false; }
	if ((View == EView::Opening || View == EView::ManualClosing) && !IsMidAnimation(View == EView::Opening))
	{ Finish(TEXT("Requested/processed transition screenshot missed the real T1 intermediate frame.")); return false; }
	if (View == EView::SpeedUp || View == EView::Selected)
	{
		const auto* Root = Feature->GetWidgetFromName(TEXT("FeatureModeCanvas"));
		if (!Root || !FMath::IsNearlyEqual(Root->GetRenderOpacity(), 1.f))
		{ Finish(TEXT("Expected settled SpeedUp screen.")); return false; }
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const auto* Text = Cast<UTextBlock>(Feature->GetWidgetFromName(FName(*FString::Printf(TEXT("SpeedUpItemDetails_%d"), Index))));
			if (!SpeedUpAuditVisible(Text) || !Text->GetText().ToString().Contains(TEXT("库存：--")))
			{ Finish(TEXT("SpeedUp directory no longer explicitly preserves unknown item inventory.")); return false; }
		}
	}
	if (View == EView::Selected)
	{
		const auto* Feedback = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("FeatureBoundaryFeedback")));
		if (!SpeedUpAuditVisible(Feedback) || !Feedback->GetText().ToString().Contains(TEXT("未改变倒计时")))
		{ Finish(TEXT("Actual item-selection delegate did not display the no-consumption boundary.")); return false; }
	}
	return true;
}

bool URokSpeedUpAuditSubsystem::Click(UUserWidget* Owner, FName Name)
{
	static const TSet<FName> Allowed = {TEXT("BuildingUpgradeButton"), TEXT("TimedUpgradeButton"), TEXT("BuildingFeatureButton"), TEXT("SpeedUpItemAction_1"), TEXT("FeatureFrameClose"), TEXT("Type2CloseButton")};
	auto* Button = Owner ? Cast<UButton>(Owner->GetWidgetFromName(Name)) : nullptr;
	if (!Allowed.Contains(Name) || !Button || Owner->GetWorld() != GetWorld() || !SpeedUpAuditVisible(Button) || !Button->GetIsEnabled() || !Validate())
	{ if (!bFinished) Finish(FString::Printf(TEXT("Forbidden/missing/disabled button %s."), *Name.ToString())); return false; }
	auto Action = MakeShared<FJsonObject>(); Action->SetStringField(TEXT("button"), Name.ToString());
	Action->SetNumberField(TEXT("frame"), double(Frame)); Action->SetNumberField(TEXT("world_seconds"), GetWorld()->GetTimeSeconds());
	Action->SetStringField(TEXT("input"), TEXT("Actual OnClicked delegate; not physical pointer input"));
	Actions.Add(MakeShared<FJsonValueObject>(Action));
	Button->OnClicked.Broadcast(); return true;
}

void URokSpeedUpAuditSubsystem::Enter(int32 NextStep) { Step = NextStep; StepStarted = Elapsed; }
TSharedRef<FJsonObject> URokSpeedUpAuditSubsystem::State() const
{
	auto Value = MakeShared<FJsonObject>(); Value->SetNumberField(TEXT("step"), Step); Value->SetNumberField(TEXT("frame"), double(Frame));
	Value->SetNumberField(TEXT("world_seconds"), GetWorld() ? GetWorld()->GetTimeSeconds() : -1);
	Value->SetNumberField(TEXT("wall_seconds"), FPlatformTime::Seconds() - StartedWall);
	if (LastHallObservation.IsValid()) Value->SetObjectField(TEXT("last_prerequisite_observation"), LastHallObservation);
	if (LastQueueObservation.IsValid()) Value->SetObjectField(TEXT("last_barracks_observation"), LastQueueObservation);
	Value->SetNumberField(TEXT("completion_transitions"), CompletionTransitions); Value->SetBoolField(TEXT("queue_bound"), bBound);
	Value->SetBoolField(TEXT("prerequisite_bound"), bHallBound); Value->SetBoolField(TEXT("prerequisite_completed"), bHallCompleted);
	Value->SetStringField(TEXT("prerequisite_actor"), GetNameSafe(HallActor.Get()));
	Value->SetNumberField(TEXT("prerequisite_started_at"), HallBoundState.UpgradeStartedAt);
	Value->SetNumberField(TEXT("prerequisite_finishes_at"), HallBoundState.UpgradeFinishesAt);
	Value->SetBoolField(TEXT("barracks_baseline_recorded"), bTargetBaselineRecorded);
	Value->SetNumberField(TEXT("completion_events"), CompletionEvents); Value->SetNumberField(TEXT("power_play_count"), PowerPlayCount);
	if (RuntimeState.IsValid())
	{
		const auto Profile = RuntimeState->GetPlayerProfileState(); Value->SetNumberField(TEXT("profile_preview_score"), Profile.LocalPreviewScore);
		Value->SetNumberField(TEXT("profile_building_delta"), Profile.LocalBuildingUpgradeScore);
		const FRokUnityRuntimeTaskState& TaskSnapshot = RuntimeState->GetActiveTask();
		Value->SetNumberField(TEXT("task_snapshot_id"), TaskSnapshot.TaskId);
		Value->SetNumberField(TEXT("task_snapshot_current"), TaskSnapshot.Current);
		Value->SetNumberField(TEXT("task_snapshot_target"), TaskSnapshot.Target);
		Value->SetBoolField(TEXT("task_snapshot_complete"), TaskSnapshot.bComplete);
	}
	if (PowerGain.IsValid())
	{
		Value->SetBoolField(TEXT("power_playing"), PowerGain->IsPlaying()); Value->SetNumberField(TEXT("power_playback_seconds"), PowerGain->GetPlaybackTime());
		Value->SetNumberField(TEXT("power_remaining_seconds"), PowerGain->GetRemainingTime());
	}
	if (Hud.IsValid())
	{
		if (const auto* Text = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("PlayerPowerText")))) Value->SetStringField(TEXT("hud_power_text"), Text->GetText().ToString());
		if (const auto* Text = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("TaskTitle_0")))) Value->SetStringField(TEXT("hud_task_title_text"), Text->GetText().ToString());
		if (const auto* Text = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("TaskProgress_0"))))
		{
			Value->SetStringField(TEXT("hud_task_progress_text"), Text->GetText().ToString());
			Value->SetStringField(TEXT("hud_task_progress_tooltip"), Text->GetToolTipText().ToString());
		}
		if (const UWidget* Reward = Hud->GetWidgetFromName(TEXT("TaskReward_0"))) Value->SetBoolField(TEXT("hud_task_reward_visible"), Reward->GetVisibility() != ESlateVisibility::Collapsed && Reward->GetVisibility() != ESlateVisibility::Hidden);
	}
	Value->SetStringField(TEXT("actor"), GetNameSafe(TargetActor.Get())); Value->SetBoolField(TEXT("feature_open"), Feature.IsValid() && Feature->IsScreenOpen());
	Value->SetNumberField(TEXT("original_started_at"), BoundState.UpgradeStartedAt); Value->SetNumberField(TEXT("original_finishes_at"), BoundState.UpgradeFinishesAt);
	if (Feature.IsValid())
	{
		if (const UWidget* Root = Feature->GetWidgetFromName(TEXT("FeatureModeCanvas")))
		{
			Value->SetNumberField(TEXT("opacity"), Root->GetRenderOpacity());
			Value->SetNumberField(TEXT("scale_x"), Root->GetRenderTransform().Scale.X);
		}
		if (const auto* Progress = Cast<UProgressBar>(Feature->GetWidgetFromName(TEXT("SpeedUpQueueProgress")))) Value->SetNumberField(TEXT("queue_progress"), Progress->GetPercent());
		if (const auto* Description = Cast<UTextBlock>(Feature->GetWidgetFromName(TEXT("SpeedUpQueueDescription")))) Value->SetStringField(TEXT("queue_text"), Description->GetText().ToString());
	}
	return Value;
}
void URokSpeedUpAuditSubsystem::Capture(const TCHAR* Name)
{
	if (!Validate()) return;
	if (FScreenshotRequest::IsScreenshotRequested()) { Finish(TEXT("Another screenshot request is already pending.")); return; }
	PendingPath = Directory / (FString(Name) + TEXT(".png"));
	if (IFileManager::Get().FileExists(*PendingPath)) { Finish(TEXT("Refusing existing screenshot evidence.")); return; }
	PendingCapture = MakeShared<FJsonObject>(); PendingCapture->SetStringField(TEXT("name"), Name); PendingCapture->SetStringField(TEXT("path"), PendingPath);
	PendingCapture->SetObjectField(TEXT("state_at_request"), State()); ScreenshotStarted = Elapsed;
	FScreenshotRequest::RequestScreenshot(PendingPath, true, false, false, FIntRect(), true);
}
void URokSpeedUpAuditSubsystem::ScreenshotProcessed()
{
	if (bFinished || !PendingCapture.IsValid() || !Validate()) return;
	TArray<uint8> Bytes; const uint8 Signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
	if (!FFileHelper::LoadFileToArray(Bytes, *PendingPath) || Bytes.Num() < 24 || FMemory::Memcmp(Bytes.GetData(), Signature, 8) != 0)
	{ Finish(TEXT("Processed screenshot is absent, empty or not a PNG.")); return; }
	const auto ReadBE = [&Bytes](int32 Offset) { return (uint32(Bytes[Offset]) << 24) | (uint32(Bytes[Offset + 1]) << 16) | (uint32(Bytes[Offset + 2]) << 8) | uint32(Bytes[Offset + 3]); };
	if (ReadBE(16) < 2 || ReadBE(20) < 2) { Finish(TEXT("Screenshot dimensions are invalid.")); return; }
	PendingCapture->SetNumberField(TEXT("file_bytes"), Bytes.Num()); PendingCapture->SetNumberField(TEXT("width"), ReadBE(16)); PendingCapture->SetNumberField(TEXT("height"), ReadBE(20));
	PendingCapture->SetObjectField(TEXT("state_at_processed"), State()); Captures.Add(MakeShared<FJsonValueObject>(PendingCapture)); PendingCapture.Reset();
	if (!Save(false)) { Finish(TEXT("Progress manifest save failed.")); return; }
	Enter(Step + 1);
}

bool URokSpeedUpAuditSubsystem::CheckTaskHoverInvariant() const
{
	if (!Hud.IsValid() || !RuntimeState.IsValid() || !Upgrades.IsValid() || !HallActor.IsValid()
		|| !bCompleted || !bHallCompleted || Upgrades->GetActiveUpgradeCount() != 0
		|| !SpeedUpAuditSameResources(Upgrades->GetCurrentResources(), ResourcesAfter)) return false;
	const FRokBuildingState Hall = Upgrades->GetBuildingStateForActor(HallActor.Get());
	const FRokUnityRuntimeTaskState Snapshot = RuntimeState->GetActiveTask();
	const FRokUnityRuntimeTaskLocalProjection Projection = RuntimeState->GetActiveTaskLocalProjection(&Hall);
	const auto* Title = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("TaskTitle_0")));
	const auto* Progress = Cast<UTextBlock>(Hud->GetWidgetFromName(TEXT("TaskProgress_0")));
	const UWidget* Reward = Hud->GetWidgetFromName(TEXT("TaskReward_0"));
	return Hall.UnityInitId == 5 && Hall.UnityBuildingTypeId == 1 && Hall.Level == 2 && !Hall.bUpgradeInProgress
		&& Snapshot.TaskId == 200900 && Snapshot.Current == 1 && Snapshot.Target == 2 && !Snapshot.bComplete
		&& Projection.bProjected && Projection.bConditionMet && Projection.Task.Current == 2
		&& !Projection.Task.bComplete && Projection.Task.NextId == Snapshot.NextId
		&& Title && Title->GetText().ToString() == TEXT("起源·本地")
		&& Progress && Progress->GetVisibility() == ESlateVisibility::Visible
		&& Progress->GetText().ToString() == TEXT("将市政厅升至等级2 (2/2)")
		&& Progress->GetToolTipText().ToString() == TEXT("本地条件已满足，服务器任务状态待同步")
		&& SpeedUpAuditVisible(Progress) && Reward && Reward->GetVisibility() == ESlateVisibility::Collapsed;
}

bool URokSpeedUpAuditSubsystem::BeginTaskHoverAudit()
{
	if (!CheckTaskHoverInvariant() || !FSlateApplication::IsInitialized())
	{ Finish(TEXT("Task hover baseline is not the completed local 2/2 state with immutable reward and wallet.")); return false; }
	FSlateApplication& App = FSlateApplication::Get();
	if (!App.GetAllowTooltips() || !App.GetCursorUser().IsValid())
	{ Finish(TEXT("Slate tooltips or cursor user are unavailable; refusing to force-enable a tooltip.")); return false; }
	TArray<TSharedRef<SWindow>> Windows;
	App.GetAllVisibleWindowsOrdered(Windows);
	for (const TSharedRef<SWindow>& Window : Windows)
	{
		if (Window->GetType() == EWindowType::ToolTip && Window->GetOpacity() > 0)
		{ Finish(TEXT("An unrelated Slate tooltip was visible before the task hover audit.")); return false; }
	}
	UTextBlock* Progress = CastChecked<UTextBlock>(Hud->GetWidgetFromName(TEXT("TaskProgress_0")));
	const FGeometry Geometry = Progress->GetCachedGeometry();
	if (Geometry.GetLocalSize().X <= 0 || Geometry.GetLocalSize().Y <= 0)
	{ Finish(TEXT("TaskProgress_0 has no cached Slate geometry.")); return false; }
	const FVector2D Point = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * 0.5f);
	const FVector2D Away = Geometry.LocalToAbsolute(FVector2D(-32.0f, -32.0f));
	TaskHoverOriginalPointer = App.GetCursorPos();
	bTaskHoverPointerSaved = true;
	// Tooltip discovery reads the cursor user's actual cursor position, not the
	// position carried only by ProcessMouseMoveEvent. Move it through Slate and
	// restore it on every exit path; never call SpawnToolTip or enable tooltips.
	App.SetCursorPos(Point);
	const FVector2D ActualHoverCursor = App.GetCursorPos();
	if (!ActualHoverCursor.Equals(Point, 2.0f))
	{ Finish(TEXT("Slate cursor could not move to TaskProgress_0 for a natural tooltip.")); return false; }
	App.ProcessMouseMoveEvent(FPointerEvent(0, Away, TaskHoverOriginalPointer, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	App.ProcessMouseMoveEvent(FPointerEvent(0, Point, Away, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	const FWidgetPath Path = App.LocateWindowUnderMouse(Point, App.GetInteractiveTopLevelWindows(), false, 0);
	if (!Path.IsValid() || !Path.ContainsWidget(&Progress->TakeWidget().Get()) || !Progress->IsHovered())
	{ Finish(TEXT("Real Slate move/hit path did not hover TaskProgress_0.")); return false; }
	const UGameViewportClient* GameViewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> GameWindow = GameViewport ? GameViewport->GetWindow() : nullptr;
	if (!GameWindow.IsValid() || GameWindow != Path.GetWindow())
	{ Finish(TEXT("TaskProgress_0 Slate hit path is not hosted by this GameApp viewport window.")); return false; }
	const bool bPlatformCursorOverBeforeFocus = App.GetPlatformApplication().IsValid()
		&& App.GetPlatformApplication()->IsCursorDirectlyOverSlateWindow();
	const TSharedRef<FJsonObject> OsBeforeFocus = SpeedUpAuditCursorWindowDiagnostics(GameWindow.ToSharedRef(), Point);
	const bool bNeedsWindowFocus = !bPlatformCursorOverBeforeFocus || !App.IsActive();
	TSharedRef<FJsonObject> OsActivation = MakeShared<FJsonObject>();
	OsActivation->SetBoolField(TEXT("attempted"), false);
	if (bNeedsWindowFocus)
	{
		// Normal OS window activation, scoped to the GameApp window that owns the
		// proven target path. Slate's next ordinary tick decides whether to show it.
		GameWindow->BringToFront(true);
		if (App.GetPlatformApplication().IsValid()
			&& !App.GetPlatformApplication()->IsCursorDirectlyOverSlateWindow())
		{
			// Windows' normal BringToFront only calls SetActiveWindow for a regular
			// window. Show/foreground this verified game HWND so WindowFromPoint can
			// satisfy Slate's unmodified, platform-owned tooltip gate.
			OsActivation = SpeedUpAuditActivateVerifiedGameWindow(GameWindow.ToSharedRef(), Point);
		}
	}
	const TSharedPtr<IToolTip> InitialTooltip = Progress->TakeWidget()->GetToolTip();
	if (!InitialTooltip.IsValid() || InitialTooltip->IsEmpty()
		|| !SpeedUpAuditSlateContainsText(InitialTooltip->AsWidget(), Progress->GetToolTipText().ToString()))
	{ Finish(TEXT("TaskProgress_0 has no matching Slate tooltip object after real hover.")); return false; }
	TaskHoverInitialTooltip = InitialTooltip;
	TaskHoverLastTooltip = InitialTooltip;
	TaskHoverInitialTooltipWidget = InitialTooltip->AsWidget();
	TaskHoverLastTooltipWidget = InitialTooltip->AsWidget();
	TaskHoverEvidence = MakeShared<FJsonObject>();
	TaskHoverEvidence->SetStringField(TEXT("method"), TEXT("FSlateApplication.ProcessMouseMoveEvent then natural tooltip wait"));
	TaskHoverEvidence->SetBoolField(TEXT("target_in_hit_path"), true);
	TaskHoverEvidence->SetBoolField(TEXT("target_hovered"), true);
	TaskHoverEvidence->SetBoolField(TEXT("platform_cursor_over_slate_before_focus"), bPlatformCursorOverBeforeFocus);
	TaskHoverEvidence->SetBoolField(TEXT("game_window_is_target_path_window"), true);
	TaskHoverEvidence->SetBoolField(TEXT("game_window_brought_to_front"), bNeedsWindowFocus);
	TaskHoverEvidence->SetObjectField(TEXT("os_before_focus"), OsBeforeFocus);
	TaskHoverEvidence->SetObjectField(TEXT("os_activation"), OsActivation);
	TaskHoverEvidence->SetObjectField(TEXT("os_after_focus"), SpeedUpAuditCursorWindowDiagnostics(GameWindow.ToSharedRef(), Point));
	TaskHoverEvidence->SetNumberField(TEXT("target_absolute_x"), Point.X);
	TaskHoverEvidence->SetNumberField(TEXT("target_absolute_y"), Point.Y);
	TaskHoverEvidence->SetStringField(TEXT("expected_tooltip"), Progress->GetToolTipText().ToString());
	TaskHoverEvidence->SetStringField(TEXT("initial_tooltip_object"), FString::Printf(TEXT("%p"), InitialTooltip.Get()));
	TaskHoverEvidence->SetStringField(TEXT("initial_tooltip_widget"), FString::Printf(TEXT("%p"), &InitialTooltip->AsWidget().Get()));
	TaskHoverEvidence->SetObjectField(TEXT("resources_at_hover"), SpeedUpAuditResources(Upgrades->GetCurrentResources()));
	TaskHoverEvidence->SetNumberField(TEXT("active_queues_at_hover"), Upgrades->GetActiveUpgradeCount());
	TaskHoverEvidence->SetObjectField(TEXT("state_at_hover"), State());
	TaskHoverStartedWall = FPlatformTime::Seconds();
	return true;
}

bool URokSpeedUpAuditSubsystem::PollTaskHoverAudit()
{
	if (!TaskHoverEvidence.IsValid() || !CheckTaskHoverInvariant())
	{ Finish(TEXT("Task hover changed completed local state, task snapshot, reward, resources or queue.")); return false; }
	FSlateApplication& App = FSlateApplication::Get();
	UTextBlock* Progress = CastChecked<UTextBlock>(Hud->GetWidgetFromName(TEXT("TaskProgress_0")));
	const FString Expected = Progress->GetToolTipText().ToString();
	if (!Progress->IsHovered() || !App.GetAllowTooltips())
	{ Finish(TEXT("Task progress lost real Slate hover or global tooltips became unavailable.")); return false; }
	const FVector2D TargetPoint = Progress->GetCachedGeometry().LocalToAbsolute(
		Progress->GetCachedGeometry().GetLocalSize() * 0.5f);
	const FVector2D ActualHoverCursor = App.GetCursorPos();
	if (!ActualHoverCursor.Equals(TargetPoint, 2.0f))
	{ Finish(TEXT("Slate cursor moved away before the natural task tooltip appeared.")); return false; }
	const TSharedPtr<IToolTip> TargetTooltip = Progress->TakeWidget()->GetToolTip();
	const bool bPlatformCursorOverSlateWindow = App.GetPlatformApplication().IsValid()
		&& App.GetPlatformApplication()->IsCursorDirectlyOverSlateWindow();
	++TaskHoverPolls;
	if (TaskHoverLastTooltip.Pin().Get() != TargetTooltip.Get()) ++TaskHoverTooltipChanges;
	TaskHoverLastTooltip = TargetTooltip;
	bool bTargetTextMatched = false;
	bool bAttachedVisiblePath = false;
	bool bViewportPresenter = false;
	bool bGameLayer = false;
	if (TargetTooltip.IsValid() && !TargetTooltip->IsEmpty())
	{
		const TSharedRef<SWidget> TooltipWidget = TargetTooltip->AsWidget();
		if (TaskHoverLastTooltipWidget.Pin().Get() != &TooltipWidget.Get()) ++TaskHoverWidgetChanges;
		TaskHoverLastTooltipWidget = TooltipWidget;
		FWidgetPath TooltipPath;
		bTargetTextMatched = SpeedUpAuditSlateContainsText(TooltipWidget, Expected);
		if (bTargetTextMatched && App.GeneratePathToWidgetUnchecked(TooltipWidget, TooltipPath, EVisibility::Visible)
			&& TooltipPath.IsValid())
		{
			bAttachedVisiblePath = true;
			const TSharedRef<SWindow> HostWindow = TooltipPath.GetWindow();
			for (int32 WidgetIndex = 0; WidgetIndex < TooltipPath.Widgets.Num(); ++WidgetIndex)
			{
				const FArrangedWidget& Arranged = TooltipPath.Widgets[WidgetIndex];
				bViewportPresenter |= Arranged.Widget->GetType() == FName(TEXT("STooltipPresenter"));
				bGameLayer |= Arranged.Widget->GetType() == FName(TEXT("SGameLayerManager"));
			}
			TArray<TSharedRef<SWindow>> VisibleWindows;
			App.GetAllVisibleWindowsOrdered(VisibleWindows);
			const bool bVisibleHost = VisibleWindows.Contains(HostWindow);
			const bool bNativeTooltipWindow = HostWindow->GetType() == EWindowType::ToolTip;
			const FVector2D TooltipDrawSize = TooltipWidget->GetCachedGeometry().GetDrawSize();
			if (bVisibleHost && bPlatformCursorOverSlateWindow && HostWindow->GetOpacity() >= 0.95f
				&& (bNativeTooltipWindow || (bViewportPresenter && bGameLayer))
				&& TooltipWidget->GetRenderOpacity() > 0.0f && TooltipDrawSize.X >= 2.0 && TooltipDrawSize.Y >= 2.0)
			{
				TArray<FColor> Pixels;
				FIntVector Size = FIntVector::ZeroValue;
				if (!App.TakeScreenshot(TooltipWidget, Pixels, Size) || Size.X < 2 || Size.Y < 2
					|| Pixels.Num() < Size.X * Size.Y)
				{ Finish(TEXT("Actual visible Slate tooltip widget could not be captured.")); return false; }
				const FString Path = Directory / TEXT("task_hover_tooltip.png");
				if (IFileManager::Get().FileExists(*Path)
					|| !FImageUtils::SaveImageByExtension(*Path, FImageView(Pixels.GetData(), Size.X, Size.Y)))
				{ Finish(TEXT("Actual Slate tooltip PNG could not be saved without overwrite.")); return false; }
				TArray<uint8> PngBytes;
				const uint8 PngSignature[] = {137, 80, 78, 71, 13, 10, 26, 10};
				if (!FFileHelper::LoadFileToArray(PngBytes, *Path) || PngBytes.Num() < 24
					|| FMemory::Memcmp(PngBytes.GetData(), PngSignature, 8) != 0)
				{ Finish(TEXT("Saved actual Slate tooltip screenshot is not a PNG.")); return false; }
				TaskHoverEvidence->SetStringField(TEXT("rendered_tooltip_text"), Expected);
				TaskHoverEvidence->SetStringField(TEXT("tooltip_host"), bNativeTooltipWindow ? TEXT("native_tooltip_window") : TEXT("game_viewport_STooltipPresenter"));
				TaskHoverEvidence->SetStringField(TEXT("tooltip_slate_path"), TooltipPath.ToString());
				TaskHoverEvidence->SetBoolField(TEXT("platform_cursor_over_slate_with_tooltip"), bPlatformCursorOverSlateWindow);
				TaskHoverEvidence->SetObjectField(TEXT("os_with_tooltip"), SpeedUpAuditCursorWindowDiagnostics(HostWindow, TargetPoint));
				TaskHoverEvidence->SetStringField(TEXT("tooltip_widget_png"), Path);
				TaskHoverEvidence->SetNumberField(TEXT("tooltip_png_bytes"), PngBytes.Num());
				TaskHoverEvidence->SetNumberField(TEXT("tooltip_host_window_opacity"), HostWindow->GetOpacity());
				TaskHoverEvidence->SetNumberField(TEXT("tooltip_widget_opacity"), TooltipWidget->GetRenderOpacity());
				TaskHoverEvidence->SetNumberField(TEXT("tooltip_absolute_x"), TooltipWidget->GetCachedGeometry().GetAbsolutePosition().X);
				TaskHoverEvidence->SetNumberField(TEXT("tooltip_absolute_y"), TooltipWidget->GetCachedGeometry().GetAbsolutePosition().Y);
				TaskHoverEvidence->SetNumberField(TEXT("tooltip_png_width"), Size.X);
				TaskHoverEvidence->SetNumberField(TEXT("tooltip_png_height"), Size.Y);
				TaskHoverEvidence->SetNumberField(TEXT("hover_wall_seconds"), FPlatformTime::Seconds() - TaskHoverStartedWall);
				TaskHoverEvidence->SetObjectField(TEXT("resources_with_visible_tooltip"), SpeedUpAuditResources(Upgrades->GetCurrentResources()));
				TaskHoverEvidence->SetNumberField(TEXT("active_queues_with_visible_tooltip"), Upgrades->GetActiveUpgradeCount());
				TaskHoverEvidence->SetObjectField(TEXT("state_with_visible_tooltip"), State());
				bTaskHoverVerified = true;
				return true;
			}
		}
	}
	if (FPlatformTime::Seconds() - TaskHoverStartedWall > 4.0)
	{
		TArray<TSharedRef<SWindow>> VisibleWindows;
		App.GetAllVisibleWindowsOrdered(VisibleWindows);
		const SWidget* TargetWidget = TargetTooltip.IsValid() ? &TargetTooltip->AsWidget().Get() : nullptr;
		int32 PresenterCount = 0, PresenterMatchingTextCount = 0, PresenterTargetWidgetCount = 0, NativeTooltipWindowCount = 0;
		for (const TSharedRef<SWindow>& Window : VisibleWindows)
		{
			if (Window->GetType() == EWindowType::ToolTip) ++NativeTooltipWindowCount;
			SpeedUpAuditInspectTooltipPresenters(Window->GetContent(), TargetWidget, Expected,
				PresenterCount, PresenterMatchingTextCount, PresenterTargetWidgetCount);
		}
		FWidgetPath AnyVisibilityPath;
		const bool bAttachedAnyVisibility = TargetWidget
			&& App.GeneratePathToWidgetUnchecked(TargetTooltip->AsWidget(), AnyVisibilityPath, EVisibility::All)
			&& AnyVisibilityPath.IsValid();
		const FWidgetPath CurrentHitPath = App.LocateWindowUnderMouse(TargetPoint, App.GetInteractiveTopLevelWindows(), false, 0);
		TaskHoverEvidence->SetNumberField(TEXT("tooltip_poll_count"), TaskHoverPolls);
		TaskHoverEvidence->SetNumberField(TEXT("tooltip_object_change_count"), TaskHoverTooltipChanges);
		TaskHoverEvidence->SetNumberField(TEXT("tooltip_widget_change_count"), TaskHoverWidgetChanges);
		TaskHoverEvidence->SetBoolField(TEXT("initial_tooltip_object_still_alive"), TaskHoverInitialTooltip.Pin().IsValid());
		TaskHoverEvidence->SetBoolField(TEXT("initial_tooltip_widget_still_alive"), TaskHoverInitialTooltipWidget.Pin().IsValid());
		TaskHoverEvidence->SetStringField(TEXT("latest_tooltip_object"), FString::Printf(TEXT("%p"), TargetTooltip.Get()));
		TaskHoverEvidence->SetStringField(TEXT("latest_tooltip_widget"), FString::Printf(TEXT("%p"), TargetWidget));
		TaskHoverEvidence->SetStringField(TEXT("tooltip_widget_parent_type"), TargetWidget && TargetWidget->GetParentWidget().IsValid()
			? TargetWidget->GetParentWidget()->GetType().ToString() : TEXT("none"));
		if (TargetWidget)
		{
			const FGeometry& TooltipGeometry = TargetWidget->GetCachedGeometry();
			TaskHoverEvidence->SetNumberField(TEXT("tooltip_cached_width"), TooltipGeometry.GetDrawSize().X);
			TaskHoverEvidence->SetNumberField(TEXT("tooltip_cached_height"), TooltipGeometry.GetDrawSize().Y);
			TaskHoverEvidence->SetNumberField(TEXT("tooltip_cached_x"), TooltipGeometry.GetAbsolutePosition().X);
			TaskHoverEvidence->SetNumberField(TEXT("tooltip_cached_y"), TooltipGeometry.GetAbsolutePosition().Y);
		}
		TaskHoverEvidence->SetBoolField(TEXT("tooltip_attached_any_visibility_path"), bAttachedAnyVisibility);
		TaskHoverEvidence->SetStringField(TEXT("tooltip_any_visibility_path"), bAttachedAnyVisibility ? AnyVisibilityPath.ToString() : TEXT("none"));
		TaskHoverEvidence->SetNumberField(TEXT("visible_slate_window_count"), VisibleWindows.Num());
		TaskHoverEvidence->SetNumberField(TEXT("visible_native_tooltip_window_count"), NativeTooltipWindowCount);
		TaskHoverEvidence->SetNumberField(TEXT("visible_tooltip_presenter_count"), PresenterCount);
		TaskHoverEvidence->SetNumberField(TEXT("presenter_matching_text_count"), PresenterMatchingTextCount);
		TaskHoverEvidence->SetNumberField(TEXT("presenter_target_widget_count"), PresenterTargetWidgetCount);
		TaskHoverEvidence->SetBoolField(TEXT("slate_application_active"), App.IsActive());
		TaskHoverEvidence->SetBoolField(TEXT("slate_high_precision_mouse"), App.IsUsingHighPrecisionMouseMovment());
		TaskHoverEvidence->SetBoolField(TEXT("slate_drag_drop"), App.IsDragDropping());
		TaskHoverEvidence->SetBoolField(TEXT("slate_left_mouse_pressed"), App.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton));
		TaskHoverEvidence->SetBoolField(TEXT("platform_cursor_over_slate_window"), bPlatformCursorOverSlateWindow);
		TaskHoverEvidence->SetBoolField(TEXT("target_in_current_hit_path"), CurrentHitPath.IsValid()
			&& CurrentHitPath.ContainsWidget(&Progress->TakeWidget().Get()));
		if (GetWorld() && GetWorld()->GetGameViewport() && GetWorld()->GetGameViewport()->GetWindow().IsValid())
		{
			TaskHoverEvidence->SetObjectField(TEXT("os_at_timeout"), SpeedUpAuditCursorWindowDiagnostics(
				GetWorld()->GetGameViewport()->GetWindow().ToSharedRef(), TargetPoint));
		}
		TaskHoverEvidence->SetBoolField(TEXT("target_tooltip_object_present"), TargetTooltip.IsValid());
		TaskHoverEvidence->SetBoolField(TEXT("target_tooltip_text_matched"), bTargetTextMatched);
		TaskHoverEvidence->SetBoolField(TEXT("target_tooltip_attached_visible_path"), bAttachedVisiblePath);
		TaskHoverEvidence->SetBoolField(TEXT("target_tooltip_viewport_presenter"), bViewportPresenter);
		TaskHoverEvidence->SetBoolField(TEXT("target_tooltip_game_layer"), bGameLayer);
		Finish(bPlatformCursorOverSlateWindow
			? TEXT("Target Slate tooltip was not visible in a native window or game viewport presenter within four real seconds.")
			: TEXT("Platform cursor never reached the verified GameApp Slate window, so natural tooltip spawning was unavailable."));
		return false;
	}
	return false;
}

void URokSpeedUpAuditSubsystem::RestoreTaskHoverPointer()
{
	if (!bTaskHoverPointerSaved || !FSlateApplication::IsInitialized()) return;
	FSlateApplication& App = FSlateApplication::Get();
	const FVector2D Current = App.GetCursorPos();
	App.SetCursorPos(TaskHoverOriginalPointer);
	App.ProcessMouseMoveEvent(FPointerEvent(0, TaskHoverOriginalPointer, Current, TSet<FKey>(),
		EKeys::Invalid, 0, FModifierKeysState()));
	App.CloseToolTip();
	bTaskHoverPointerSaved = false;
	if (TaskHoverEvidence.IsValid()) TaskHoverEvidence->SetBoolField(TEXT("pointer_and_tooltip_restored"), true);
}

void URokSpeedUpAuditSubsystem::Tick(float DeltaTime)
{
	if (bFinished || !GetWorld()->HasBegunPlay()) return;
	Elapsed += FMath::Max(DeltaTime, 0.f); ++Frame;
	if (FPlatformTime::Seconds() - StartedWall > 90) { Finish(TEXT("Natural queue audit exceeded 90 seconds.")); return; }
	if (bBegun && (!CheckIdentity() || !CheckHallQueue() || !CheckQueue() || !CheckPower())) return;
	if (PendingCapture.IsValid()) { if (Elapsed - ScreenshotStarted > 8) Finish(TEXT("Screenshot callback timed out.")); return; }
	const double Delay = Step == 0 ? 3.0 : (Step == 6 || Step == 11 ? .04 : (Step >= 15 && Step <= 18 ? 0.0 : .35));
	if (Elapsed - StepStarted < Delay) return;
	switch (Step)
	{
	case 0: if (Begin()) Capture(TEXT("01_city_source_barracks")); break;
	case 1: if (Click(Panel.Get(), TEXT("BuildingUpgradeButton"))) { View = EView::BlockedUpgrade; Enter(100); } break;
	case 2: Capture(TEXT("05_source_upgrade_20_seconds")); break;
	case 3:
		UpgradeClickedWall = FPlatformTime::Seconds();
		if (Click(Modal.Get(), TEXT("TimedUpgradeButton"))) { bTargetStartRequested = true; View = EView::City; Enter(4); } break;
	case 4: if (BindActualQueue()) Capture(TEXT("06_real_queue_started")); break;
	case 5: if (Click(Panel.Get(), TEXT("BuildingFeatureButton"))) { View = EView::Opening; Enter(6); } break;
	case 6: Capture(TEXT("07_t1_opening")); break;
	case 7: View = EView::SpeedUp; Capture(TEXT("08_queue_countdown")); break;
	case 8: if (Click(Feature.Get(), TEXT("SpeedUpItemAction_1"))) { View = EView::Selected; Enter(9); } break;
	case 9: Capture(TEXT("09_directory_selection_no_time_change")); break;
	case 10: if (Click(Feature.Get(), TEXT("FeatureFrameClose"))) { View = EView::ManualClosing; Enter(11); } break;
	case 11: Capture(TEXT("10_t1_manual_close")); break;
	case 12:
		View = EView::City;
		if (Validate()) { bDismissed = true; if (Click(Panel.Get(), TEXT("BuildingFeatureButton"))) { View = EView::SpeedUp; Enter(13); } } break;
	case 13:
		if (!bDismissed) { Finish(TEXT("SpeedUp was not observed closed before reopening.")); break; }
		Capture(TEXT("11_reopened_original_queue")); break;
	case 14:
		if (bCompleted) { Finish(TEXT("Missed active queue capture before natural completion.")); break; }
		if (BoundState.UpgradeFinishesAt - GetWorld()->GetTimeSeconds() <= 2.f) Capture(TEXT("12_before_natural_completion")); break;
	case 15:
		if (bCompleted && IsMidAnimation()) { View = EView::NaturalClosing; Capture(TEXT("13_t1_natural_completion")); }
		else if (bCompleted && (!Feature->IsScreenOpen() || FPlatformTime::Seconds() - CompletionWall > 2)) Finish(TEXT("Natural completion never produced the source closing transition."));
		break;
	case 16:
		if (!Feature->IsScreenOpen() && PowerGain->IsPlaying() && PowerGain->GetPlaybackTime() >= .1f)
		{ View = EView::PowerEarly; Capture(TEXT("14_natural_power_gain_early")); }
		else if (FPlatformTime::Seconds() - CompletionWall > 1) Finish(TEXT("Missed early real power-gain playback."));
		break;
	case 17:
		if (PowerGain->IsPlaying() && PowerGain->GetPlaybackTime() >= .5f)
		{ View = EView::PowerLater; Capture(TEXT("15_natural_power_gain_later")); }
		else if (FPlatformTime::Seconds() - CompletionWall > 1.4) Finish(TEXT("Missed later real power-gain playback."));
		break;
	case 18:
		if (FPlatformTime::Seconds() - CompletionWall >= 1.5) { View = EView::CompletedCity; Capture(TEXT("16_completed_once_hud_score")); } break;
	case 19:
		if (Validate())
		{
			if (SelectExistingBuilding(WorkerActor.Get())) Enter(20);
		} break;
	case 20: if (Click(Panel.Get(), TEXT("BuildingFeatureButton"))) { View = EView::Worker; Enter(21); } break;
	case 21: Capture(TEXT("17_actual_worker_queue_idle")); break;
	case 22: if (Click(Feature.Get(), TEXT("FeatureFrameClose"))) { View = EView::WorkerClosing; Enter(23); } break;
	case 23:
		View = EView::CompletedCity;
		if (Validate())
		{
			if (bTaskHoverAuditRequested) Enter(24);
			else Finish();
		}
		break;
	case 24: if (BeginTaskHoverAudit()) Enter(25); break;
	case 25: if (PollTaskHoverAudit()) Finish(); break;
	case 100: Capture(TEXT("02_barracks_blocked_cityhall_requirement")); break;
	case 101: if (Click(Modal.Get(), TEXT("Type2CloseButton"))) { View = EView::City; Enter(102); } break;
	case 102: if (SelectExistingBuilding(HallActor.Get())) Enter(103); break;
	case 103: if (Click(Panel.Get(), TEXT("BuildingUpgradeButton"))) { View = EView::HallUpgrade; Enter(104); } break;
	case 104:
		HallClickedWall = FPlatformTime::Seconds();
		if (Click(Modal.Get(), TEXT("TimedUpgradeButton"))) { bHallStartRequested = true; View = EView::City; Enter(105); } break;
	case 105: if (BindHallQueue()) { View = EView::HallActive; Capture(TEXT("03_real_cityhall_prerequisite_queue")); } break;
	case 106:
		if (bHallCompleted && FPlatformTime::Seconds() - HallCompletionWall >= 1.5)
		{ View = EView::HallComplete; Capture(TEXT("04_cityhall_completed_power_plus14")); } break;
	case 107:
		if (Validate())
		{
			BaselineScore = RuntimeState->GetPlayerProfileState().LocalPreviewScore;
			ResourcesBefore = Upgrades->GetCurrentResources();
			if (BaselineScore != InitialScore + 14 || CompletionEvents != 1 || !SpeedUpAuditSameResources(ResourcesBefore, ResourcesAfterHall))
			{ Finish(TEXT("Cannot record a legal barracks baseline before prerequisite completion settles.")); break; }
			bTargetBaselineRecorded = true;
			if (SelectExistingBuilding(TargetActor.Get())) { View = EView::City; Enter(108); }
		} break;
	case 108: if (Click(Panel.Get(), TEXT("BuildingUpgradeButton"))) { View = EView::Upgrade; Enter(2); } break;
	default: Finish(TEXT("Unexpected SpeedUp audit stage.")); break;
	}
}

bool URokSpeedUpAuditSubsystem::Save(bool bComplete)
{
	auto Root = MakeShared<FJsonObject>(); Root->SetNumberField(TEXT("audit_version"), 5); Root->SetBoolField(TEXT("complete"), bComplete);
	Root->SetBoolField(TEXT("passed"), bComplete && Failures.IsEmpty() && Captures.Num() == SpeedUpAuditCaptureCount && CompletionTransitions == 1 && bHallCompleted && bTargetBaselineRecorded && CompletionEvents == 2 && PowerPlayCount == 2 && (!bTaskHoverAuditRequested || bTaskHoverVerified));
	Root->SetBoolField(TEXT("task_hover_requested"), bTaskHoverAuditRequested);
	Root->SetBoolField(TEXT("task_hover_verified"), bTaskHoverVerified);
	if (TaskHoverEvidence.IsValid()) Root->SetObjectField(TEXT("task_hover"), TaskHoverEvidence);
	else Root->SetField(TEXT("task_hover"), MakeShared<FJsonValueNull>());
	Root->SetNumberField(TEXT("expected_capture_count"), SpeedUpAuditCaptureCount); Root->SetNumberField(TEXT("completion_transitions"), CompletionTransitions);
	Root->SetStringField(TEXT("project"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Root->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetOutermost()->GetName() : TEXT("unavailable"));
	Root->SetStringField(TEXT("scope"), TEXT("Existing Init6 initially blocked by source row902; existing Init5 performs row102 for two real seconds, then Init6 performs row902 for twenty real seconds. Source prerequisites/costs and local completion power are verified, total delta19/two events. No authoritative inventory, speedup consumption, server queue or complete server validation claim."));
	Root->SetStringField(TEXT("input_method"), bTaskHoverAuditRequested
		? TEXT("Existing queue interaction uses real UMG delegates; optional task tooltip check uses real Slate pointer-move hit path and the naturally presented target tooltip widget. No time/animation advance or fake queue fixture.")
		: TEXT("Initial existing controller AutoSelect fixture; real UMG delegates and normal ticks. Existing Init5/Init6/Init2 selection uses the public HUD selection setter; every queue starts through actual upgrade buttons. No physical pointer/hit-test claim; no time/animation advance or fake queue fixture."));
	Root->SetStringField(TEXT("source"), TEXT("UI_Win_AddSpeed.prefab -> UA_WinAnime_T1; AddSpeedMediator.InitData/OnTimerComplete/OnSliderChange; UI_Win_WorkerMediator.OnSpeedUp1BtnClick -> CityBuildingProxy.AddBuildingUpdateSpeed; Role_SpeedUp remains server-owned."));
	Root->SetBoolField(TEXT("unity_visual_comparison_complete"), false);
	Root->SetStringField(TEXT("queue_clock_semantics"), TEXT("BuildingSubsystem compares float(World.GetTimeSeconds()) to float queue timestamps; raw double and exact float projection are recorded separately, with no epsilon or time advance."));
	Root->SetNumberField(TEXT("source_queue_seconds"), 20); Root->SetNumberField(TEXT("source_building_level_id"), 902);
	Root->SetNumberField(TEXT("natural_completion_wall_seconds"), bCompleted ? CompletionWall - UpgradeClickedWall : -1);
	Root->SetNumberField(TEXT("initial_preview_score"), InitialScore); Root->SetNumberField(TEXT("baseline_preview_score"), BaselineScore);
	Root->SetNumberField(TEXT("prerequisite_source_row"), 102); Root->SetNumberField(TEXT("prerequisite_source_seconds"), 2);
	Root->SetNumberField(TEXT("prerequisite_natural_wall_seconds"), bHallCompleted ? HallCompletionWall - HallClickedWall : -1);
	Root->SetNumberField(TEXT("expected_total_local_power_increment"), 19); Root->SetNumberField(TEXT("expected_local_power_increment"), 5);
	Root->SetNumberField(TEXT("completion_events"), CompletionEvents); Root->SetNumberField(TEXT("power_play_count"), PowerPlayCount); Root->SetArrayField(TEXT("power_samples"), PowerSamples);
	if (bBegun) Root->SetObjectField(TEXT("resources_before_prerequisite"), SpeedUpAuditResources(ResourcesInitial));
	else Root->SetField(TEXT("resources_before_prerequisite"), MakeShared<FJsonValueNull>());
	if (bHallBound) Root->SetObjectField(TEXT("resources_after_prerequisite_debit"), SpeedUpAuditResources(ResourcesAfterHall));
	else Root->SetField(TEXT("resources_after_prerequisite_debit"), MakeShared<FJsonValueNull>());
	if (bTargetBaselineRecorded) Root->SetObjectField(TEXT("resources_before_timed_upgrade"), SpeedUpAuditResources(ResourcesBefore));
	else Root->SetField(TEXT("resources_before_timed_upgrade"), MakeShared<FJsonValueNull>());
	if (bBound) Root->SetObjectField(TEXT("resources_after_one_source_debit"), SpeedUpAuditResources(ResourcesAfter));
	else Root->SetField(TEXT("resources_after_one_source_debit"), MakeShared<FJsonValueNull>());
	Root->SetObjectField(TEXT("last_observed_state"), State()); Root->SetArrayField(TEXT("actions"), Actions);
	Root->SetArrayField(TEXT("queue_samples"), QueueSamples); Root->SetArrayField(TEXT("captures"), Captures);
	TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& Error : Failures) Errors.Add(MakeShared<FJsonValueString>(Error)); Root->SetArrayField(TEXT("failures"), Errors);
	FString Json; if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json))) return false;
	return FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("manifest.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
void URokSpeedUpAuditSubsystem::Finish(const FString& Failure)
{
	if (bFinished) return;
	if (!Failure.IsEmpty()) Failures.Add(Failure);
	RestoreTaskHoverPointer();
	if (bTaskHoverVerified && !CheckTaskHoverInvariant())
		Failures.Add(TEXT("Task hover cleanup changed the completed local task/wallet/queue state."));
	if (bTaskHoverAuditRequested && !bTaskHoverVerified && Failures.IsEmpty())
		Failures.Add(TEXT("Requested task progress hover was not verified by a visible Slate tooltip window."));
	if (Failures.IsEmpty() && (Captures.Num() != SpeedUpAuditCaptureCount || CompletionTransitions != 1 || !bHallCompleted || !bTargetBaselineRecorded || CompletionEvents != 2 || PowerPlayCount != 2)) Failures.Add(TEXT("Incomplete capture/completion/power evidence."));
	if (!Save(Failures.IsEmpty())) Failures.Add(TEXT("Final manifest save failed."));
	bFinished = true;
	UE_LOG(LogTemp, Display, TEXT("Rok SpeedUp audit passed=%d captures=%d completions=%d output=%s"), Failures.IsEmpty(), Captures.Num(), CompletionTransitions, *Directory);
	for (const auto& Error : Failures) UE_LOG(LogTemp, Error, TEXT("Rok SpeedUp audit: %s"), *Error);
	FPlatformMisc::RequestExitWithStatus(false, Failures.IsEmpty() ? 0 : 1);
}
