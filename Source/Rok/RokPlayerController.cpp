#include "RokPlayerController.h"

#include "Components/InputComponent.h"
#include "Components/ScrollBox.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokAudioRouterSubsystem.h"
#include "RokBuildingActor.h"
#include "RokBuildingSelectionIndicator.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokBuildingPanelWidget.h"
#include "RokCityGridMath.h"
#include "RokCityPlacementFirstFit.h"
#include "RokCityPlacementPreviewActor.h"
#include "RokCityPlacementPreviewMath.h"
#include "RokCitySpriteActor.h"
#include "RokMainHudWidget.h"
#include "RokStrategyCameraPawn.h"
#include "RokUnityFeatureScreenWidget.h"
#include "Layout/WidgetPath.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace
{
#if WITH_EDITOR
constexpr int32 DefaultBuildingGridDebugMode = 2;
#else
constexpr int32 DefaultBuildingGridDebugMode = 0;
#endif

TAutoConsoleVariable<int32> CVarRokDebugBuildingGrid(
	TEXT("rok.DebugBuildingGrid"),
	DefaultBuildingGridDebugMode,
	TEXT("Draw Unity-aligned building footprint grids: 0=off, 1=selected, 2=all selectable buildings."),
	ECVF_Cheat);

constexpr float BuildingGridDebugZOffset = 2.0f;
constexpr double PlacementReadinessRefreshIntervalSeconds = 0.15;
}

ARokPlayerController::ARokPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

void ARokPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
	if (UWorld* World = GetWorld())
	{
		BuildingSelectionIndicator = World->SpawnActor<ARokBuildingSelectionIndicator>();
	}

	if (!FParse::Param(FCommandLine::Get(), TEXT("RokNoHud")) && !MainHudWidget)
	{
		MainHudWidget = CreateWidget<URokMainHudWidget>(this, URokMainHudWidget::StaticClass());
		if (MainHudWidget)
		{
			MainHudWidget->AddToViewport(20);
			UE_LOG(LogTemp, Log, TEXT("Rok UI: Main HUD added to viewport."));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Rok UI: Failed to create Main HUD widget."));
		}
	}
	if (MainHudWidget)
	{
		if (URokUnityFeatureScreenWidget* Feature = MainHudWidget->GetFeatureScreenWidgetForPlacement())
		{
			BuildCityPlacementDelegateHandle = Feature->OnBuildCityPlacementRequested.AddUObject(
				this, &ARokPlayerController::HandleBuildCityPlacementRequested);
		}
		PlacementCancelDelegateHandle = MainHudWidget->OnPlacementCancelRequested.AddUObject(
			this, &ARokPlayerController::HandlePlacementCancelRequested);
		PlacementConfirmDelegateHandle = MainHudWidget->OnPlacementConfirmRequested.AddUObject(
			this, &ARokPlayerController::HandlePlacementConfirmRequested);
	}

	UE_LOG(LogTemp, Log, TEXT("Rok selection: Unity footprint grid debug mode=%d (0=off, 1=selected, 2=all)."),
		CVarRokDebugBuildingGrid.GetValueOnGameThread());
	AutoSelectBuildingFromCommandLine();
	StartSelectedBuildingUpgradeFromCommandLine();
	if (MainHudWidget)
	{
		MainHudWidget->OpenCommandLineModal();
	}
	// Unity WorldMgrMediator/CityGlobalMediator select source events 1007/1008.
	// Until the city server clock is migrated this startup is an explicit local
	// day preview; the night switch is an opt-in preview fixture.
	if (IsLocalController() && GetWorld()
		&& UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()) == TEXT("/Game/Maps/L_RokUnityGameAppCity"))
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (URokAudioRouterSubsystem* Audio = GameInstance->GetSubsystem<URokAudioRouterSubsystem>())
			{
				const bool bNightPreview = FParse::Param(FCommandLine::Get(), TEXT("RokCityBgmNight"));
				bStartedCityBgmForMap = Audio->SetCityBgmNight(bNightPreview);
				UE_LOG(LogTemp, Log, TEXT("Rok city audio local preview: night=%d started=%d; server day/night synchronization pending."),
					bNightPreview, bStartedCityBgmForMap);
			}
		}
	}
	ScheduleAutoScreenshot();
}

void ARokPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPlacementPreview(false);
	if (MainHudWidget)
	{
		if (URokUnityFeatureScreenWidget* Feature = MainHudWidget->GetFeatureScreenWidgetForPlacement())
		{
			Feature->OnBuildCityPlacementRequested.Remove(BuildCityPlacementDelegateHandle);
		}
		MainHudWidget->OnPlacementCancelRequested.Remove(PlacementCancelDelegateHandle);
		MainHudWidget->OnPlacementConfirmRequested.Remove(PlacementConfirmDelegateHandle);
	}
	if (bStartedCityBgmForMap)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (URokAudioRouterSubsystem* Audio = GameInstance->GetSubsystem<URokAudioRouterSubsystem>())
			{
				Audio->StopCityBgm();
			}
		}
		bStartedCityBgmForMap = false;
	}
	Super::EndPlay(EndPlayReason);
}

FRokCityPlacementRuntimeSnapshot ARokPlayerController::GetCityPlacementPreviewSnapshot() const
{
	FRokCityPlacementRuntimeSnapshot Snapshot;
	Snapshot.bActive = bPlacementActive;
	Snapshot.bDragging = PlacementDragState.bActive;
	Snapshot.bGridValid = bPlacementGridValid;
	Snapshot.bCanCommit = bPlacementCanCommit;
	Snapshot.bCameraInputSuspended = PlacementLockedCamera.IsValid()
		&& PlacementLockedCamera->IsCityPlacementDragInputSuspended();
	Snapshot.bAwaitPointerRelease = bPlacementAwaitPointerRelease;
	Snapshot.SuppressPrimaryClickThroughFrame = SuppressPrimaryClickThroughFrame;
	Snapshot.PrimaryPressEventCount = PrimaryPressEventCount;
	Snapshot.LastPrimaryPressFrame = LastPrimaryPressFrame;
	Snapshot.LastPrimaryPressOutcome = LastPrimaryPressOutcome;
	Snapshot.PlacementDragBeginCount = PlacementDragBeginCount;
	Snapshot.LastPlacementDragBeginFrame = LastPlacementDragBeginFrame;
	Snapshot.PlacementPointerReleaseCount = PlacementPointerReleaseCount;
	Snapshot.LastPlacementPointerReleaseFrame = LastPlacementPointerReleaseFrame;
	Snapshot.PlacementEscapeCancelCount = PlacementEscapeCancelCount;
	Snapshot.LastPlacementEscapeCancelFrame = LastPlacementEscapeCancelFrame;
	Snapshot.ConfirmClickCount = ConfirmClickCount;
	Snapshot.LastConfirmClickFrame = LastConfirmClickFrame;
	Snapshot.LastConfirmOutcome = LastConfirmOutcome;
	Snapshot.LastConfirmFailure = LastConfirmFailure;
	Snapshot.LastConfirmedLocalConstructionId = LastConfirmedLocalConstructionId;
	Snapshot.LastConfirmedActor = LastConfirmedActor;
	Snapshot.LastPlacementPressFrame = LastPlacementPressFrame;
	Snapshot.LastPlacementPressFailure = LastPlacementPressFailure;
	Snapshot.LastPlacementPressError = LastPlacementPressError;
	Snapshot.LastPlacementPressMouse = LastPlacementPressMouse;
	Snapshot.LastPlacementPressGround = LastPlacementPressGround;
	Snapshot.LastPlacementPressLocalHit = LastPlacementPressLocalHit;
	Snapshot.LastPlacementPressLocalCenter = LastPlacementPressLocalCenter;
	Snapshot.TypeId = PlacementTypeId;
	Snapshot.Grid = PlacementGrid;
	Snapshot.Footprint = PlacementFootprint;
	Snapshot.PreviewActor = PlacementPreviewActor.Get();
	Snapshot.PriorSelection = SelectionBeforePlacement.Get();
	Snapshot.CurrentSelection = SelectedBuildingActor.Get();
	return Snapshot;
}

void ARokPlayerController::StartSelectedBuildingUpgradeFromCommandLine()
{
	if (!FParse::Param(
			FCommandLine::Get(),
			TEXT("RokStartSelectedBuildingUpgrade")))
	{
		return;
	}

	if (!SelectedBuildingActor.IsValid())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok acceptance: cannot start selected-building upgrade; no building is selected."));
		return;
	}

	URokBuildingUpgradeSubsystem* UpgradeSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>()
		: nullptr;
	if (!UpgradeSubsystem)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok acceptance: cannot start selected-building upgrade; subsystem unavailable."));
		return;
	}

	FString FailureReason;
	if (UpgradeSubsystem->StartUpgrade(
			SelectedBuildingActor.Get(),
			FailureReason))
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok acceptance: command line started selected-building upgrade actor=%s."),
			*GetNameSafe(SelectedBuildingActor.Get()));
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok acceptance: selected-building upgrade was rejected: %s"),
			*FailureReason);
	}
}

void ARokPlayerController::AutoSelectBuildingFromCommandLine()
{
	FString Selector;
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokAutoSelectBuilding="), Selector) || Selector.IsEmpty())
	{
		return;
	}

	for (TActorIterator<ARokCitySpriteActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		ARokCitySpriteActor* Candidate = *ActorIt;
		bool bMatches = GetActorDisplayString(Candidate).Contains(Selector, ESearchCase::IgnoreCase);
		for (const FName& Tag : Candidate->Tags)
		{
			bMatches |= Tag.ToString().Equals(Selector, ESearchCase::IgnoreCase);
		}
		if (bMatches && IsSelectableBuilding(Candidate))
		{
			UE_LOG(LogTemp, Log, TEXT("Rok selection: command line selected %s using '%s'."),
				*Candidate->GetName(), *Selector);
			SelectBuilding(Candidate);
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Rok selection: no building matched command line selector '%s'."), *Selector);
}

void ARokPlayerController::ScheduleAutoScreenshot()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokAutoScreenshot")))
	{
		return;
	}

	float DelaySeconds = 5.0f;
	FParse::Value(FCommandLine::Get(), TEXT("RokScreenshotDelay="), DelaySeconds);
	DelaySeconds = FMath::Clamp(DelaySeconds, 1.0f, 30.0f);
	GetWorldTimerManager().SetTimer(AutoScreenshotTimerHandle, this, &ARokPlayerController::TakeAutoScreenshot, DelaySeconds, false);
	UE_LOG(LogTemp, Log, TEXT("Rok screenshot: scheduled auto capture in %.2f seconds."), DelaySeconds);
}

void ARokPlayerController::TakeAutoScreenshot()
{
#if !UE_BUILD_SHIPPING
	float BuildCitySample = 0.0f;
	float PowerSample = 0.0f;
	if (MainHudWidget && FParse::Value(FCommandLine::Get(), TEXT("RokHudPowerSample="), PowerSample))
	{
		// Display-only capture fixture; never changes the player profile.
		MainHudWidget->SetPowerPresentation(-1);
		MainHudWidget->SetPowerPresentation(6200);
		MainHudWidget->SetPowerPresentation(6400);
		// Offline diagnostic sampling only; preserve first-update particle birth.
		float SampleRemaining = FMath::Clamp(PowerSample, 0.0f, 1.2f);
		while (SampleRemaining > 0.0f)
		{
			const float Step = FMath::Min(SampleRemaining, 1.0f / 120.0f);
			MainHudWidget->AdvanceHudAnimations(Step);
			SampleRemaining -= Step;
		}
		UE_LOG(LogTemp, Display, TEXT("Rok power HUD capture: requested_sample=%.6f; real HUD tick may advance before screenshot."), PowerSample);
	}
	float ChatScrollSample = 0.0f;
	if (MainHudWidget && FParse::Value(FCommandLine::Get(), TEXT("RokChatScrollSample="), ChatScrollSample))
	{
		if (auto* Feature = Cast<URokUnityFeatureScreenWidget>(MainHudWidget->GetWidgetFromName(TEXT("UnityFeatureScreenWidget"))))
			if (auto* Scroll = Cast<UScrollBox>(Feature->GetWidgetFromName(TEXT("ChatMessageScroll"))))
			{
				Scroll->SetScrollOffset(FMath::Clamp(ChatScrollSample, 0.0f, Scroll->GetScrollOffsetOfEnd()));
				UE_LOG(LogTemp, Log, TEXT("Rok chat scroll capture: offset=%.2f end=%.2f"), Scroll->GetScrollOffset(), Scroll->GetScrollOffsetOfEnd());
			}
	}
	float ChatUnreadSample = 0.0f;
	if (MainHudWidget && FParse::Value(FCommandLine::Get(), TEXT("RokHudChatUnreadSample="), ChatUnreadSample))
	{
		// Explicit diagnostic only: no fabricated membership/unread state in normal play.
		MainHudWidget->SetChatChannelPresentation(true, false, 1);
		MainHudWidget->AdvanceHudAnimations(FMath::Clamp(ChatUnreadSample, 0.0f, 1.0f));
		UE_LOG(LogTemp, Log, TEXT("Rok HUD chat unread diagnostic sample %.3f"), ChatUnreadSample);
	}
	if (MainHudWidget && FParse::Value(FCommandLine::Get(), TEXT("RokBuildCityAnimationSample="), BuildCitySample))
	{
		if (auto* Feature = Cast<URokUnityFeatureScreenWidget>(MainHudWidget->GetWidgetFromName(TEXT("UnityFeatureScreenWidget"))))
			Feature->SampleBuildCityAnimationForCapture(BuildCitySample);
	}
	float MenuCloseSample = 0.0f;
	if (MainHudWidget && FParse::Value(FCommandLine::Get(), TEXT("RokBuildingMenuCloseSample="), MenuCloseSample))
	{
		if (auto* Menu = Cast<URokBuildingPanelWidget>(MainHudWidget->GetWidgetFromName(TEXT("BuildingPanelWidget"))))
		{
			Menu->SetSelectedBuilding(nullptr);
			Menu->AdvanceMenuAnimation(FMath::Clamp(MenuCloseSample, 0.0f, 0.25f));
			UE_LOG(LogTemp, Log, TEXT("Rok building menu close capture: sampled %.3fs"), MenuCloseSample);
		}
	}
#endif
	FString ScreenshotPath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokScreenshotPath="), ScreenshotPath) || ScreenshotPath.IsEmpty())
	{
		ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("RokCityRuntime_AutoScreenshot.png");
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
	FString NormalizedScreenshotPath = ScreenshotPath;
	FPaths::NormalizeFilename(NormalizedScreenshotPath);
	if (FParse::Param(FCommandLine::Get(), TEXT("RokScreenshotSceneOnly")))
	{
		int32 ScreenshotWidth = 1600;
		int32 ScreenshotHeight = 1000;
		FParse::Value(FCommandLine::Get(), TEXT("RokScreenshotWidth="), ScreenshotWidth);
		FParse::Value(FCommandLine::Get(), TEXT("RokScreenshotHeight="), ScreenshotHeight);
		ScreenshotWidth = FMath::Clamp(ScreenshotWidth, 320, 7680);
		ScreenshotHeight = FMath::Clamp(ScreenshotHeight, 200, 4320);
		ConsoleCommand(FString::Printf(
			TEXT("HighResShot %dx%d filename=%s"),
			ScreenshotWidth,
			ScreenshotHeight,
			*NormalizedScreenshotPath));
		UE_LOG(LogTemp, Log, TEXT("Rok screenshot: requested scene-only HighResShot %dx%d %s"), ScreenshotWidth, ScreenshotHeight, *NormalizedScreenshotPath);
	}
	else
	{
		// UI-inclusive captures use the live viewport size. Set -ResX/-ResY on
		// the process when an exact comparison resolution is required.
		FScreenshotRequest::RequestScreenshot(NormalizedScreenshotPath, true, false, false, FIntRect(), true);
		UE_LOG(LogTemp, Log, TEXT("Rok screenshot: requested UI-inclusive viewport capture %s"), *NormalizedScreenshotPath);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("RokScreenshotQuit")))
	{
		GetWorldTimerManager().SetTimer(AutoScreenshotQuitTimerHandle, this, &ARokPlayerController::QuitAfterAutoScreenshot, 5.0f, false);
	}
}

void ARokPlayerController::QuitAfterAutoScreenshot()
{
	ConsoleCommand(TEXT("quit"));
}

void ARokPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ARokPlayerController::HandlePrimaryClick);
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this,
			&ARokPlayerController::HandlePlacementPointerReleased);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this,
			&ARokPlayerController::HandlePlacementEscape);
		UE_LOG(LogTemp, Log, TEXT("Rok selection: primary click binding installed on %s."), *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Rok selection: no InputComponent; primary click cannot be bound."));
	}
}

void ARokPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (bPlacementAwaitPointerRelease && !IsInputKeyDown(EKeys::LeftMouseButton))
	{
		bPlacementAwaitPointerRelease = false;
	}
	if (bPlacementActive || bPlacementFeedbackOnly)
	{
		if (MainHudWidget)
		{
			if (MainHudWidget->IsPlacementBlockingModalOpen())
			{
				CancelPlacementPreview();
			}
			if (URokUnityFeatureScreenWidget* Feature = MainHudWidget->GetFeatureScreenWidgetForPlacement())
			{
				if (Feature->IsScreenOpen())
				{
					CancelPlacementPreview();
				}
			}
		}
		if (bPlacementActive && PlacementDragState.bActive)
		{
			UpdatePlacementDrag();
		}
		else if (bPlacementActive)
		{
			const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
			if (LastPlacementReadinessCheckSeconds < 0.0
				|| Now < LastPlacementReadinessCheckSeconds
				|| Now - LastPlacementReadinessCheckSeconds >= PlacementReadinessRefreshIntervalSeconds)
			{
				RefreshPlacementCommitReadiness();
			}
		}
	}
	DrawBuildingGridDebug();
}

void ARokPlayerController::HandlePrimaryClick()
{
	++PrimaryPressEventCount;
	LastPrimaryPressFrame = GFrameCounter;
	if (GFrameCounter <= SuppressPrimaryClickThroughFrame)
	{
		LastPrimaryPressOutcome = TEXT("suppressed");
		return;
	}
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	const bool bHasMousePosition = GetMousePosition(MouseX, MouseY);
	if (!bHasMousePosition)
	{
		LastPrimaryPressOutcome = TEXT("no_mouse");
		return;
	}
	if (IsPointerOverRokUi())
	{
		LastPrimaryPressOutcome = TEXT("ui_owned");
		return;
	}
	if (bPlacementActive)
	{
		if (!bPlacementAwaitPointerRelease)
		{
			LastPrimaryPressOutcome = TEXT("placement_dispatched");
			HandlePlacementPointerPressed();
		}
		else
		{
			LastPrimaryPressOutcome = TEXT("placement_await");
		}
		return;
	}
	LastPrimaryPressOutcome = TEXT("selection");
	UE_LOG(LogTemp, Log, TEXT("Rok selection: primary click received (mouse=%s %.1f,%.1f)."),
		bHasMousePosition ? TEXT("valid") : TEXT("invalid"), MouseX, MouseY);

	AActor* HitActor = FindSelectableBuildingUnderCursor();
	if (!HitActor)
	{
		UE_LOG(LogTemp, Log, TEXT("Rok selection: no selectable building under cursor."));
		ClearSelection();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Rok selection: selected %s."), *HitActor->GetName());
	SelectBuilding(HitActor);
}

bool ARokPlayerController::IsPointerOverRokUi() const
{
	if (!FSlateApplication::IsInitialized() || !MainHudWidget)
	{
		return bPlacementActive;
	}
	FSlateApplication& Slate = FSlateApplication::Get();
	const FWidgetPath Path = Slate.LocateWindowUnderMouse(
		Slate.GetCursorPos(), Slate.GetInteractiveTopLevelWindows());
	if (!Path.IsValid())
	{
		return true; // Ambiguous ownership must not start a world drag.
	}
	if (Path.ContainsWidget(&MainHudWidget->TakeWidget().Get()))
	{
		return true;
	}
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		if (Path.Widgets[Index].Widget->GetTypeAsString().Contains(TEXT("SObjectWidget")))
		{
			return true; // Another UMG surface owns this pointer.
		}
	}
	return false;
}

void ARokPlayerController::PresentPlacementFailure(const FString& Reason)
{
	bPlacementFeedbackOnly = true;
	if (MainHudWidget)
	{
		MainHudWidget->SetPlacementPresentation(true, false, false, FText::FromString(Reason));
	}
	UE_LOG(LogTemp, Warning, TEXT("Rok city placement preview refused: %s"), *Reason);
}

void ARokPlayerController::HandleBuildCityPlacementRequested(int32 UnityBuildingTypeId)
{
	CancelPlacementPreview();
	SuppressPrimaryClickThroughFrame = GFrameCounter + 1;
	if (UnityBuildingTypeId != 2 && UnityBuildingTypeId != 3)
	{
		PresentPlacementFailure(TEXT("本轮仅支持农场和木材厂的临时摆放预览。"));
		return;
	}
	URokBuildingUpgradeSubsystem* Buildings = GetWorld()
		? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
	FRokLocalConstructionPreview Entry;
	if (!Buildings || !Buildings->PreviewLocalConstructionEntry(UnityBuildingTypeId, Entry)
		|| !Entry.bContextVerified || !Entry.bSourceEligible
		|| Entry.Footprint.X != 5 || Entry.Footprint.Y != 5)
	{
		PresentPlacementFailure(!Entry.FailureReason.IsEmpty()
			? Entry.FailureReason : TEXT("源建造入口或一级占地尚未通过校验。"));
		return;
	}
	FRokCityPlacementWorldFrame Frame;
	FString Error;
	if (!FRokCityPlacementWorldFrame::ResolveForWorld(GetWorld(), Frame, Error))
	{
		PresentPlacementFailure(Error);
		return;
	}
	FIntPoint FirstGrid;
	const bool bFound = FRokCityPlacementFirstFit::Find(Frame.GetGridSize(), Entry.Footprint,
		[this, Buildings, UnityBuildingTypeId, &Entry](const FIntPoint& Candidate)
		{
			FRokLocalConstructionRequest Request;
			Request.UnityBuildingTypeId = UnityBuildingTypeId;
			Request.CivilizationId = Entry.VisualCivilization;
			Request.Age = Entry.ResolvedAge;
			Request.Grid = Candidate;
			FRokLocalConstructionPreview Preview;
			return Buildings->PreviewLocalConstruction(Request, Preview)
				&& Preview.bContextVerified && Preview.bPositionEvaluated
				&& Preview.bSourceEligible && Preview.Footprint == Entry.Footprint;
		}, FirstGrid);
	if (!bFound)
	{
		PresentPlacementFailure(TEXT("当前城市源格图没有可用的五乘五摆放位置。"));
		return;
	}
	FRokLocalConstructionRequest Request;
	Request.UnityBuildingTypeId = UnityBuildingTypeId;
	Request.CivilizationId = Entry.VisualCivilization;
	Request.Age = Entry.ResolvedAge;
	Request.Grid = FirstGrid;
	FRokLocalConstructionPreview Positioned;
	if (!Buildings->PreviewLocalConstruction(Request, Positioned)
		|| !Positioned.bContextVerified || !Positioned.bPositionEvaluated
		|| !Positioned.bSourceEligible)
	{
		PresentPlacementFailure(!Positioned.FailureReason.IsEmpty()
			? Positioned.FailureReason : TEXT("初始摆放位置已变化。"));
		return;
	}
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARokCityPlacementPreviewActor* PreviewActor = GetWorld()->SpawnActor<ARokCityPlacementPreviewActor>(
		ARokCityPlacementPreviewActor::StaticClass(), FTransform::Identity, Spawn);
	if (!PreviewActor || !PreviewActor->InitializePreview(UnityBuildingTypeId,
			Entry.VisualCivilization, Entry.ResolvedAge, Error))
	{
		if (PreviewActor) PreviewActor->Destroy();
		PresentPlacementFailure(Error.IsEmpty()
			? TEXT("源建筑预览视觉资产尚未就绪。") : Error);
		return;
	}
	const FRokCityConstructionFootprint Footprint{FirstGrid, Entry.Footprint};
	PreviewActor->SetCandidate(Frame.GetCityContainerTransform(), Footprint, true);
	PlacementPreviewActor = PreviewActor;
	PlacementWorldFrame = Frame;
	PlacementGrid = FirstGrid;
	PlacementFootprint = Entry.Footprint;
	PlacementTypeId = UnityBuildingTypeId;
	PlacementVisualCivilization = Entry.VisualCivilization;
	PlacementAge = Entry.ResolvedAge;
	bPlacementGridValid = true;
	bPlacementCanCommit = Positioned.bCanCommit;
	LastPlacementPreviewReason = Positioned.FailureReason;
	bPlacementActive = true;
	LastPlacementReadinessCheckSeconds = GetWorld()->GetRealTimeSeconds();
	bPlacementFeedbackOnly = false;
	bPlacementAwaitPointerRelease = true; // The BuildCity card's own click cannot start a world drag.
	SelectionBeforePlacement = SelectedBuildingActor;
	// Unity CityGlobalMediator.CreateTempBuild -> ShowMenu(temp, true) replaces
	// the previous building HUD once the temporary actor exists. The separate
	// UE placement menu now owns that target; retire the old selection/menu.
	ClearSelection();
	if (MainHudWidget)
	{
		MainHudWidget->SetPlacementPresentation(true, true, bPlacementCanCommit,
			FText::FromString(Positioned.FailureReason));
	}
	UE_LOG(LogTemp, Display,
		TEXT("Rok city placement preview: type=%d grid=(%d,%d) source model=%s; no ticket or resource change."),
		UnityBuildingTypeId, FirstGrid.X, FirstGrid.Y, *Positioned.SourceModelId);
}

void ARokPlayerController::SetPlacementCameraInputSuspended(bool bSuspended)
{
	if (bSuspended)
	{
		if (PlacementLockedCamera.IsValid()) return;
		ARokStrategyCameraPawn* Camera = Cast<ARokStrategyCameraPawn>(GetPawn());
		if (!Camera) return;
		PlacementLockedCamera = Camera;
		bPlacementPriorCameraSuspended = Camera->IsCityPlacementDragInputSuspended();
		Camera->SetCityPlacementDragInputSuspended(true);
		return;
	}
	if (ARokStrategyCameraPawn* Camera = PlacementLockedCamera.Get())
	{
		Camera->SetCityPlacementDragInputSuspended(bPlacementPriorCameraSuspended);
	}
	PlacementLockedCamera.Reset();
	bPlacementPriorCameraSuspended = false;
}

void ARokPlayerController::HandlePlacementPointerPressed()
{
	LastPlacementPressFrame = GFrameCounter;
	LastPlacementPressFailure = NAME_None;
	LastPlacementPressError.Reset();
	LastPlacementPressMouse = FVector2D::ZeroVector;
	LastPlacementPressGround = FVector::ZeroVector;
	LastPlacementPressLocalHit = FVector2D::ZeroVector;
	LastPlacementPressLocalCenter = FVector2D::ZeroVector;
	if (!bPlacementActive || !PlacementPreviewActor.IsValid()
		|| !Cast<ARokStrategyCameraPawn>(GetPawn()))
	{
		LastPlacementPressFailure = TEXT("placement_actor_or_camera_unavailable");
		return;
	}
	float MouseX = 0, MouseY = 0;
	FString Error;
	FVector Ground;
	FVector2D LocalHit, LocalCenter;
	if (!GetMousePosition(MouseX, MouseY))
	{
		LastPlacementPressFailure = TEXT("no_mouse");
		return;
	}
	LastPlacementPressMouse = FVector2D(MouseX, MouseY);
	if (!PlacementWorldFrame.TryScreenToGround(*this, LastPlacementPressMouse, Ground, Error))
	{
		LastPlacementPressFailure = TEXT("screen_to_ground");
		LastPlacementPressError = Error;
		return;
	}
	LastPlacementPressGround = Ground;
	if (!FRokCityPlacementPreviewMath::WorldToCityLocal(
			PlacementWorldFrame.GetCityContainerTransform(), Ground, LocalHit, Error))
	{
		LastPlacementPressFailure = TEXT("world_to_city_local");
		LastPlacementPressError = Error;
		return;
	}
	LastPlacementPressLocalHit = LocalHit;
	const FRokCityConstructionFootprint Footprint{PlacementGrid, PlacementFootprint};
	if (!FRokCityPlacementPreviewMath::GridToLocalCenter(Footprint, LocalCenter, Error))
	{
		LastPlacementPressFailure = TEXT("grid_to_local_center");
		LastPlacementPressError = Error;
		return;
	}
	LastPlacementPressLocalCenter = LocalCenter;
	if (FMath::Abs(LocalHit.X - LocalCenter.X) > PlacementFootprint.X * 0.05
		|| FMath::Abs(LocalHit.Y - LocalCenter.Y) > PlacementFootprint.Y * 0.05)
	{
		LastPlacementPressFailure = TEXT("outside_footprint");
		return;
	}
	if (!FRokCityPlacementPreviewMath::BeginDrag(PlacementDragState, Footprint,
			PlacementWorldFrame.GetCityContainerTransform(), Ground, false, Error))
	{
		LastPlacementPressFailure = TEXT("begin_drag");
		LastPlacementPressError = Error;
		return;
	}
	++PlacementDragBeginCount;
	LastPlacementDragBeginFrame = GFrameCounter;
	SetPlacementCameraInputSuspended(true);
}

void ARokPlayerController::UpdatePlacementDrag()
{
	if (!bPlacementActive || !PlacementDragState.bActive) return;
	if (!IsInputKeyDown(EKeys::LeftMouseButton))
	{
		HandlePlacementPointerReleased();
		return;
	}
	float MouseX = 0, MouseY = 0;
	if (!GetMousePosition(MouseX, MouseY)
		|| IsPointerOverRokUi()) return;
	FVector Ground;
	FString Error;
	if (!PlacementWorldFrame.TryScreenToGround(
		*this, FVector2D(MouseX, MouseY), Ground, Error)) return;
	bool bGridChanged = false;
	if (!FRokCityPlacementPreviewMath::MoveDrag(PlacementDragState,
		PlacementWorldFrame.GetCityContainerTransform(), Ground, false, bGridChanged, Error)
		|| !bGridChanged) return;
	PlacementGrid = PlacementDragState.Footprint.Origin;
	PlacementSubmitFailure.Reset();
	RefreshPlacementCommitReadiness();
}

void ARokPlayerController::RefreshPlacementCommitReadiness()
{
	if (!bPlacementActive) return;
	LastPlacementReadinessCheckSeconds = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	FRokLocalConstructionRequest Request;
	Request.UnityBuildingTypeId = PlacementTypeId;
	Request.CivilizationId = PlacementVisualCivilization;
	Request.Age = PlacementAge;
	Request.Grid = PlacementGrid;
	FRokLocalConstructionPreview Preview;
	URokBuildingUpgradeSubsystem* Buildings = GetWorld()
		? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
	if (!Buildings || !Buildings->PreviewLocalConstruction(Request, Preview)
		|| !Preview.bContextVerified)
	{
		CancelPlacementPreview();
		PresentPlacementFailure(!Preview.FailureReason.IsEmpty()
			? Preview.FailureReason : TEXT("本地城市预览上下文已失效。"));
		return;
	}
	if (!PlacementSubmitFailure.IsEmpty()
		&& (bPlacementCanCommit != Preview.bCanCommit
			|| LastPlacementPreviewReason != Preview.FailureReason))
	{
		PlacementSubmitFailure.Reset();
	}
	LastPlacementPreviewReason = Preview.FailureReason;
	bPlacementGridValid = Preview.bPositionEvaluated && Preview.bSourceEligible;
	bPlacementCanCommit = Preview.bCanCommit;
	if (ARokCityPlacementPreviewActor* Actor = PlacementPreviewActor.Get())
	{
		Actor->SetCandidate(PlacementWorldFrame.GetCityContainerTransform(),
			FRokCityConstructionFootprint{PlacementGrid, PlacementFootprint}, bPlacementGridValid);
	}
	if (MainHudWidget)
	{
		MainHudWidget->SetPlacementPresentation(true, bPlacementGridValid, bPlacementCanCommit,
			FText::FromString(PlacementSubmitFailure.IsEmpty()
				? Preview.FailureReason : PlacementSubmitFailure));
	}
}

void ARokPlayerController::HandlePlacementPointerReleased()
{
	++PlacementPointerReleaseCount;
	LastPlacementPointerReleaseFrame = GFrameCounter;
	bPlacementAwaitPointerRelease = false;
	if (!PlacementDragState.bActive) return;
	FRokCityPlacementPreviewMath::CancelDrag(PlacementDragState);
	SetPlacementCameraInputSuspended(false);
}

void ARokPlayerController::HandlePlacementEscape()
{
	if (bPlacementActive || bPlacementFeedbackOnly)
	{
		if (bPlacementActive && PlacementPreviewActor.IsValid())
		{
			++PlacementEscapeCancelCount;
			LastPlacementEscapeCancelFrame = GFrameCounter;
		}
		CancelPlacementPreview();
	}
}

void ARokPlayerController::HandlePlacementCancelRequested()
{
	CancelPlacementPreview();
}

void ARokPlayerController::HandlePlacementConfirmRequested()
{
	++ConfirmClickCount;
	LastConfirmClickFrame = GFrameCounter;
	LastConfirmFailure.Reset();
	if (bPlacementConfirmInProgress)
	{
		LastConfirmOutcome = TEXT("confirm_in_progress");
		return;
	}
	if (!bPlacementActive || !PlacementPreviewActor.IsValid())
	{
		LastConfirmOutcome = TEXT("inactive");
		return;
	}
	if (PlacementDragState.bActive || bPlacementAwaitPointerRelease
		|| !MainHudWidget || MainHudWidget->IsPlacementBlockingModalOpen())
	{
		LastConfirmOutcome = TEXT("blocked_ui");
		return;
	}
	if (const URokUnityFeatureScreenWidget* Feature = MainHudWidget->GetFeatureScreenWidgetForPlacement())
	{
		if (Feature->IsScreenOpen())
		{
			LastConfirmOutcome = TEXT("blocked_ui");
			return;
		}
	}
	FRokLocalConstructionRequest Request;
	Request.UnityBuildingTypeId = PlacementTypeId;
	Request.CivilizationId = PlacementVisualCivilization;
	Request.Age = PlacementAge;
	Request.Grid = PlacementGrid;
	FRokLocalConstructionPreview Preview;
	URokBuildingUpgradeSubsystem* Buildings = GetWorld()
		? GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>() : nullptr;
	if (!Buildings || !Buildings->PreviewLocalConstruction(Request, Preview)
		|| !Preview.bContextVerified || !Preview.bPositionEvaluated
		|| !Preview.bSourceEligible || !Preview.bCanCommit)
	{
		LastConfirmOutcome = TEXT("invalid_preview");
		LastConfirmFailure = !Preview.FailureReason.IsEmpty()
			? Preview.FailureReason : TEXT("本地建造条件尚未满足。");
		if (Buildings)
		{
			RefreshPlacementCommitReadiness();
		}
		else
		{
			bPlacementGridValid = false;
			bPlacementCanCommit = false;
		}
		if (bPlacementActive && MainHudWidget)
		{
			MainHudWidget->SetPlacementPresentation(true, bPlacementGridValid, false,
				FText::FromString(LastConfirmFailure));
		}
		bPlacementCanCommit = false;
		if (bPlacementActive) PlacementSubmitFailure = LastConfirmFailure;
		return;
	}
	bPlacementConfirmInProgress = true;
	PlacementSubmitFailure.Reset();
	uint64 LocalConstructionId = 0;
	FString FailureReason;
	const bool bStarted = Buildings->StartLocalConstruction(Request, LocalConstructionId, FailureReason);
	bPlacementConfirmInProgress = false;
	if (!bStarted)
	{
		LastConfirmOutcome = TEXT("authority_rejected");
		LastConfirmFailure = FailureReason.IsEmpty()
			? TEXT("本地建造提交失败，请重新选择位置。") : FailureReason;
		RefreshPlacementCommitReadiness();
		if (bPlacementActive && MainHudWidget)
		{
			PlacementSubmitFailure = LastConfirmFailure;
			MainHudWidget->SetPlacementPresentation(true, bPlacementGridValid, bPlacementCanCommit,
				FText::FromString(LastConfirmFailure));
		}
		return;
	}
	LastConfirmedLocalConstructionId = LocalConstructionId;
	ARokCitySpriteActor* PendingActor = Buildings->GetLocalConstructionActor(LocalConstructionId);
	LastConfirmedActor = PendingActor;
	const bool bValidPending = LocalConstructionId != 0 && IsValid(PendingActor)
		&& PendingActor->GetWorld() == GetWorld()
		&& Buildings->IsLocalConstructionPending(LocalConstructionId);
	LastConfirmOutcome = bValidPending ? TEXT("committed") : TEXT("contract_violation");
	if (!bValidPending)
	{
		LastConfirmFailure = TEXT("建造已提交，但未返回有效的本地待建视觉实例。");
		UE_LOG(LogTemp, Error, TEXT("Local construction commit returned no pending source actor (id=%llu)."),
			static_cast<unsigned long long>(LocalConstructionId));
	}
	// Pending construction is not a selectable completed source building.
	CancelPlacementPreview(false);
	ClearSelection();
}

void ARokPlayerController::CancelPlacementPreview(bool bRestoreSelection)
{
	if (!bPlacementActive && !bPlacementFeedbackOnly && !PlacementPreviewActor.IsValid()) return;
	SuppressPrimaryClickThroughFrame = GFrameCounter + 1;
	HandlePlacementPointerReleased();
	FRokCityPlacementPreviewMath::CancelDrag(PlacementDragState);
	SetPlacementCameraInputSuspended(false);
	if (ARokCityPlacementPreviewActor* Actor = PlacementPreviewActor.Get()) Actor->Destroy();
	PlacementPreviewActor.Reset();
	PlacementWorldFrame = FRokCityPlacementWorldFrame();
	PlacementGrid = PlacementFootprint = FIntPoint::ZeroValue;
	PlacementTypeId = PlacementVisualCivilization = PlacementAge = 0;
	bPlacementGridValid = false;
	bPlacementCanCommit = false;
	bPlacementActive = false;
	LastPlacementReadinessCheckSeconds = -1.0;
	bPlacementFeedbackOnly = false;
	PlacementSubmitFailure.Reset();
	LastPlacementPreviewReason.Reset();
	if (MainHudWidget)
	{
		MainHudWidget->SetPlacementPresentation(false, false, false, FText::GetEmpty());
	}
	AActor* Prior = SelectionBeforePlacement.Get();
	SelectionBeforePlacement.Reset();
	if (bRestoreSelection)
	{
		if (Prior && SelectedBuildingActor.Get() == Prior)
		{
			SelectBuilding(Prior);
		}
		else if (!SelectedBuildingActor.IsValid())
		{
			ClearSelection();
		}
	}
}

AActor* ARokPlayerController::FindSelectableBuildingUnderCursor() const
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return nullptr;
	}

	if (ARokCitySpriteActor* GridBuilding = FindCityBuildingByUnityFootprint(FVector2D(MouseX, MouseY)))
	{
		const FIntPoint FootprintSize = GridBuilding->GetFootprintSize();
		UE_LOG(LogTemp, Log, TEXT("Rok selection: Unity footprint hit %s (%dx%d cells)."),
			*GridBuilding->GetName(), FootprintSize.X, FootprintSize.Y);
		return GridBuilding;
	}

	FHitResult HitResult;
	if (GetHitResultUnderCursor(ECC_Visibility, false, HitResult) && HitResult.GetActor())
	{
		if (IsSelectableBuilding(HitResult.GetActor()))
		{
			UE_LOG(LogTemp, Log, TEXT("Rok selection: Unity footprint missed; using visibility hit %s."),
				*HitResult.GetActor()->GetName());
			return HitResult.GetActor();
		}

		UE_LOG(LogTemp, Log, TEXT("Rok selection: visibility fallback blocked by %s.%s."),
			*HitResult.GetActor()->GetName(),
			HitResult.GetComponent() ? *HitResult.GetComponent()->GetName() : TEXT("<none>"));
	}

	return nullptr;
}

ARokCitySpriteActor* ARokPlayerController::FindCityBuildingByUnityFootprint(
	const FVector2D& ScreenPosition) const
{
	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ZeroVector;
	if (!DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, RayOrigin, RayDirection)
		|| FMath::IsNearlyZero(RayDirection.Z))
	{
		return nullptr;
	}

	ARokCitySpriteActor* BestActor = nullptr;
	float BestRayDistance = TNumericLimits<float>::Max();
	for (TActorIterator<ARokCitySpriteActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		ARokCitySpriteActor* Candidate = *ActorIt;
		if (!Candidate || Candidate->IsHidden() || !IsSelectableBuilding(Candidate))
		{
			continue;
		}

		const FVector Anchor = Candidate->GetVisualAnchorWorldPosition();
		const float RayDistance = (Anchor.Z - RayOrigin.Z) / RayDirection.Z;
		if (RayDistance < 0.0f || RayDistance >= BestRayDistance)
		{
			continue;
		}

		const FVector PlaneHit = RayOrigin + RayDirection * RayDistance;
		if (FRokCityGridMath::IsPointInsideFootprint(
			PlaneHit,
			Anchor,
			Candidate->GetFootprintSize()))
		{
			BestActor = Candidate;
			BestRayDistance = RayDistance;
		}
	}

	return BestActor;
}

void ARokPlayerController::DrawBuildingGridDebug() const
{
	const int32 DebugMode = FMath::Clamp(CVarRokDebugBuildingGrid.GetValueOnGameThread(), 0, 2);
	if (DebugMode == 0 || !GetWorld())
	{
		return;
	}

	const ARokCitySpriteActor* SelectedCityBuilding = Cast<ARokCitySpriteActor>(SelectedBuildingActor.Get());
	if (DebugMode == 1)
	{
		if (SelectedCityBuilding)
		{
			DrawBuildingGridDebugForActor(*SelectedCityBuilding, FColor::Yellow, 3.0f);
		}
		return;
	}

	ARokCitySpriteActor* HoveredCityBuilding = nullptr;
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (GetMousePosition(MouseX, MouseY))
	{
		HoveredCityBuilding = FindCityBuildingByUnityFootprint(FVector2D(MouseX, MouseY));
	}

	for (TActorIterator<ARokCitySpriteActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		ARokCitySpriteActor* Candidate = *ActorIt;
		if (!Candidate || Candidate->IsHidden() || !IsSelectableBuilding(Candidate))
		{
			continue;
		}

		const bool bSelected = Candidate == SelectedCityBuilding;
		const bool bHovered = Candidate == HoveredCityBuilding;
		const FColor Color = bSelected ? FColor::Yellow : (bHovered ? FColor::Magenta : FColor::Cyan);
		DrawBuildingGridDebugForActor(*Candidate, Color, bSelected || bHovered ? 3.0f : 1.0f);
	}
}

void ARokPlayerController::DrawBuildingGridDebugForActor(
	const ARokCitySpriteActor& BuildingActor,
	const FColor& Color,
	float Thickness) const
{
	const FIntPoint FootprintSize = BuildingActor.GetFootprintSize();
	if (FootprintSize.X <= 0 || FootprintSize.Y <= 0)
	{
		return;
	}

	const FVector Center = BuildingActor.GetVisualAnchorWorldPosition()
		+ FVector(0.0f, 0.0f, BuildingGridDebugZOffset);
	const FVector WidthAxis = FRokCityGridMath::GetWidthAxis();
	const FVector LengthAxis = FRokCityGridMath::GetLengthAxis();
	const float HalfWidth = FootprintSize.X * FRokCityGridMath::HalfCellSize;
	const float HalfLength = FootprintSize.Y * FRokCityGridMath::HalfCellSize;

	for (int32 WidthLine = 0; WidthLine <= FootprintSize.X; ++WidthLine)
	{
		const float WidthOffset = -HalfWidth + WidthLine * FRokCityGridMath::CellSize;
		const FVector LineCenter = Center + WidthAxis * WidthOffset;
		DrawDebugLine(
			GetWorld(),
			LineCenter - LengthAxis * HalfLength,
			LineCenter + LengthAxis * HalfLength,
			Color,
			false,
			0.0f,
			SDPG_Foreground,
			Thickness);
	}

	for (int32 LengthLine = 0; LengthLine <= FootprintSize.Y; ++LengthLine)
	{
		const float LengthOffset = -HalfLength + LengthLine * FRokCityGridMath::CellSize;
		const FVector LineCenter = Center + LengthAxis * LengthOffset;
		DrawDebugLine(
			GetWorld(),
			LineCenter - WidthAxis * HalfWidth,
			LineCenter + WidthAxis * HalfWidth,
			Color,
			false,
			0.0f,
			SDPG_Foreground,
			Thickness);
	}
}

void ARokPlayerController::SelectBuilding(AActor* Actor)
{
	if (!Actor)
	{
		ClearSelection();
		return;
	}

	if (SelectedBuildingActor.Get() == Actor)
	{
		if (BuildingSelectionIndicator)
		{
			BuildingSelectionIndicator->ShowForBuilding(Cast<ARokCitySpriteActor>(Actor));
		}
		if (MainHudWidget)
		{
			MainHudWidget->SetSelectedBuilding(Actor);
		}
		return;
	}

	ApplySelectionHighlight(SelectedBuildingActor.Get(), false);
	SelectedBuildingActor = Actor;
	ApplySelectionHighlight(Actor, true);
	if (BuildingSelectionIndicator)
	{
		BuildingSelectionIndicator->ShowForBuilding(Cast<ARokCitySpriteActor>(Actor));
	}
	if (MainHudWidget)
	{
		MainHudWidget->SetSelectedBuilding(Actor);
	}
}

void ARokPlayerController::ClearSelection()
{
	ApplySelectionHighlight(SelectedBuildingActor.Get(), false);
	SelectedBuildingActor.Reset();
	if (BuildingSelectionIndicator)
	{
		BuildingSelectionIndicator->HideIndicator();
	}
	if (MainHudWidget)
	{
		MainHudWidget->SetSelectedBuilding(nullptr);
	}
}

void ARokPlayerController::ApplySelectionHighlight(AActor* Actor, bool bHighlighted)
{
	if (!Actor)
	{
		return;
	}

	if (bHighlighted)
	{
		if (ARokBuildingActor* RokBuildingActor = Cast<ARokBuildingActor>(Actor))
		{
			RokBuildingActor->SetSelected(true);
			return;
		}
		if (ARokCitySpriteActor* CitySpriteActor = Cast<ARokCitySpriteActor>(Actor))
		{
			CitySpriteActor->SetSelected(true);
			return;
		}

		SelectedBuildingOriginalScale = Actor->GetActorScale3D();
		bHasSelectedBuildingOriginalScale = true;
		Actor->SetActorScale3D(SelectedBuildingOriginalScale * 1.035f);
	}
	else
	{
		if (ARokBuildingActor* RokBuildingActor = Cast<ARokBuildingActor>(Actor))
		{
			RokBuildingActor->SetSelected(false);
			return;
		}
		if (ARokCitySpriteActor* CitySpriteActor = Cast<ARokCitySpriteActor>(Actor))
		{
			CitySpriteActor->SetSelected(false);
			return;
		}

		if (bHasSelectedBuildingOriginalScale)
		{
			Actor->SetActorScale3D(SelectedBuildingOriginalScale);
			bHasSelectedBuildingOriginalScale = false;
		}
	}

	TInlineComponentArray<UStaticMeshComponent*> MeshComponents;
	Actor->GetComponents(MeshComponents);
	for (UStaticMeshComponent* MeshComponent : MeshComponents)
	{
		if (!MeshComponent)
		{
			continue;
		}

		MeshComponent->SetRenderCustomDepth(bHighlighted);
		MeshComponent->SetCustomDepthStencilValue(bHighlighted ? 2 : 0);
	}
}

bool ARokPlayerController::IsSelectableBuilding(AActor* Actor) const
{
	if (!Actor || Actor->Tags.Contains(TEXT("RokPlacementPreviewTemporary"))
		|| Actor->Tags.Contains(TEXT("RokLocalConstructionPending")))
	{
		return false;
	}

	if (Actor->IsA<ARokBuildingActor>())
	{
		return true;
	}

	const FString Label = GetActorDisplayString(Actor);
	if (Actor->Tags.Contains(FName(TEXT("RokBuilding"))) || Actor->Tags.Contains(FName(TEXT("CityBuilding"))))
	{
		return true;
	}

#if WITH_EDITOR
	const FString ActorFolderPath = Actor->GetFolderPath().ToString();
	if (ActorFolderPath.StartsWith(TEXT("RokPrototype/ReferenceModels/City"))
		|| ActorFolderPath.StartsWith(TEXT("RokPrototype/UnityReferenceScene/Volumetric"))
		|| ActorFolderPath.StartsWith(TEXT("RokPrototype/City"))
		|| ActorFolderPath.StartsWith(TEXT("RokPrototype/Resources")))
	{
		return true;
	}
#endif

	return Label.StartsWith(TEXT("RokLayout_"))
		|| Label.StartsWith(TEXT("OriginalTownCenter"))
		|| Label.StartsWith(TEXT("OriginalCity"))
		|| Label.StartsWith(TEXT("AllianceCenter"))
		|| Label.Contains(TEXT("Barracks"))
		|| Label.Contains(TEXT("Stable"))
		|| Label.Contains(TEXT("Archery"))
		|| Label.Contains(TEXT("Hospital"))
		|| Label.Contains(TEXT("Farm"))
		|| Label.Contains(TEXT("Lumber"))
		|| Label.Contains(TEXT("Quarry"))
		|| Label.Contains(TEXT("Gold"))
		|| Label.Contains(TEXT("Storehouse"))
		|| Label.Contains(TEXT("Tavern"));
}

FString ARokPlayerController::GetActorDisplayString(AActor* Actor) const
{
	if (!Actor)
	{
		return FString();
	}
#if WITH_EDITOR
	return Actor->GetActorLabel();
#else
	return Actor->GetName();
#endif
}
