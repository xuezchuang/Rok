#include "RokWorldMapPlayerController.h"
#include "RokMainHudWidget.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/RichTextBlock.h"

#include "Components/InputComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapCityActor.h"
#include "RokWorldMapConfigActor.h"
#include "RokWorldMapEntityActor.h"
#include "RokWorldMapHudWidget.h"
#include "RokWorldMapTypes.h"
#include "TimerManager.h"
#include "UnrealClient.h"

ARokWorldMapPlayerController::ARokWorldMapPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

bool ARokWorldMapPlayerController::CanSelectWorldObjectAtDxf(float Dxf)
{
	// PVPGlobalMediator.OnTouche3DEnd: crrLevel <= Tactical.
	// GameApp.unity camera m_lod_array[1] = 3000, inclusive in GetLodLevelByDxf.
	return FMath::IsFinite(Dxf) && Dxf >= 0.0f && Dxf <= 3000.0f;
}

void ARokWorldMapPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (const auto* Camera = Cast<ARokWorldMapCameraPawn>(GetPawn()))
		if (SelectedWorldMapActor.IsValid() && !CanSelectWorldObjectAtDxf(Camera->GetCurrentDxf()))
			ClearWorldMapSelection();
}

void ARokWorldMapPlayerController::BeginPlay()
{
	Super::BeginPlay();
	bShowMouseCursor = true;
	FInputModeGameAndUI InitialInputMode;
	InitialInputMode.SetHideCursorDuringCapture(false);
	InitialInputMode.SetLockMouseToViewportBehavior(
		EMouseLockMode::DoNotLock);
	SetInputMode(InitialInputMode);

	if (!FParse::Param(FCommandLine::Get(), TEXT("RokNoHud"))
		&& !WorldMapHudWidget)
	{
		WorldMapHudWidget = CreateWidget<URokWorldMapHudWidget>(
			this,
			URokWorldMapHudWidget::StaticClass());
		if (WorldMapHudWidget)
		{
			WorldMapHudWidget->AddToViewport(30);
			// Re-apply GameAndUI after the Slate widget exists.  Setting the
			// mode before AddToViewport leaves Standalone runs without an
			// explicit UI focus target on some window/focus transitions.
			FInputModeGameAndUI HudInputMode;
			HudInputMode.SetWidgetToFocus(
				WorldMapHudWidget->TakeWidget());
			HudInputMode.SetHideCursorDuringCapture(false);
			HudInputMode.SetLockMouseToViewportBehavior(
				EMouseLockMode::DoNotLock);
			SetInputMode(HudInputMode);
			WorldMapHudWidget->OpenCommandLineModal();
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok world-map HUD added to viewport with GameAndUI focus."));
		}
		else
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Rok world-map HUD creation failed."));
		}
	}
	ScheduleAutoScreenshot();
	ApplyCommandLineFocus();
}

void ARokWorldMapPlayerController::ApplyCommandLineFocus()
{
	float FocusX = -1.0f;
	float FocusY = -1.0f;
	FParse::Value(FCommandLine::Get(), TEXT("RokWorldMapFocusX="), FocusX);
	FParse::Value(FCommandLine::Get(), TEXT("RokWorldMapFocusY="), FocusY);
	if (FocusX < 0.0f || FocusY < 0.0f)
	{
		return;
	}
	const FVector2D DisplayCoordinate(FocusX, FocusY);
	// Defer until the camera pawn has resolved its runtime config; during
	// controller BeginPlay the pawn may still sit on the fallback
	// projection, and a map-clamped fallback focus is silently identical
	// to the default view.
	TargetFocusDisplay = DisplayCoordinate;
	GetWorldTimerManager().SetTimer(
		FocusTimerHandle,
		this,
		&ARokWorldMapPlayerController::ApplyDeferredCommandLineFocus,
		0.5f,
		false);
}

void ARokWorldMapPlayerController::ApplyDeferredCommandLineFocus()
{
	if (ARokWorldMapCameraPawn* CameraPawn =
			Cast<ARokWorldMapCameraPawn>(GetPawn()))
	{
		CameraPawn->FocusDisplayCoordinate(TargetFocusDisplay);
		float DxfOverride = -1.0f;
		if (FParse::Value(
				FCommandLine::Get(),
				TEXT("RokWorldMapDxf="),
				DxfOverride)
			&& DxfOverride > 0.0f)
		{
			CameraPawn->SetCurrentDxf(DxfOverride);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok world-map focus: dxf override %.1f -> actual %.1f."),
				DxfOverride,
				CameraPawn->GetCurrentDxf());
		}
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok world-map focus: moved camera to display (%.1f, %.1f)."),
			TargetFocusDisplay.X,
			TargetFocusDisplay.Y);
	}
}

void ARokWorldMapPlayerController::ScheduleAutoScreenshot()
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("RokAutoScreenshot")))
	{
		return;
	}

	float DelaySeconds = 5.0f;
	FParse::Value(
		FCommandLine::Get(),
		TEXT("RokScreenshotDelay="),
		DelaySeconds);
	DelaySeconds = FMath::Clamp(DelaySeconds, 1.0f, 30.0f);
	GetWorldTimerManager().SetTimer(
		AutoScreenshotTimerHandle,
		this,
		&ARokWorldMapPlayerController::TakeAutoScreenshot,
		DelaySeconds,
		false);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map screenshot: scheduled in %.2f seconds."),
		DelaySeconds);
}

void ARokWorldMapPlayerController::TakeAutoScreenshot()
{
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("RokSelectionLodAudit")) && RunSelectionLodAudit()) return;
	float TaskFoldSample = 0.0f;
	float FeatureFoldSample = 0.0f;
	if (WorldMapHudWidget && FParse::Value(FCommandLine::Get(), TEXT("RokFeatureFoldSample="), FeatureFoldSample))
	{
		if (auto* Tactical = Cast<URokMainHudWidget>(WorldMapHudWidget->GetWidgetFromName(TEXT("WorldMapTacticalHud"))))
		{
			if (auto* Fold = Cast<UButton>(Tactical->GetWidgetFromName(TEXT("MoreFeatureButton"))))
			{
				Fold->OnClicked.Broadcast();
				Tactical->AdvanceHudAnimations(FMath::Max(0.0f, FeatureFoldSample));
				UE_LOG(LogTemp, Display, TEXT("Rok feature fold capture: source button clicked, sample=%.3f"), FeatureFoldSample);
			}
		}
	}
	if (WorldMapHudWidget && FParse::Value(FCommandLine::Get(), TEXT("RokTaskFoldSample="), TaskFoldSample))
	{
		if (auto* Tactical = Cast<URokMainHudWidget>(WorldMapHudWidget->GetWidgetFromName(TEXT("WorldMapTacticalHud"))))
		{
			if (auto* Fold = Cast<UButton>(Tactical->GetWidgetFromName(TEXT("TaskFoldButton"))))
			{
				Fold->OnClicked.Broadcast();
				Tactical->AdvanceHudAnimations(FMath::Max(0.0f, TaskFoldSample));
				UE_LOG(LogTemp, Display, TEXT("Rok task fold capture: source button clicked, sample=%.3f"), TaskFoldSample);
			}
		}
	}
	// Capture a real runtime module transition after startup has settled.
	// Normal gameplay never takes this diagnostic branch.
	float MiniMapSample = 0.0f;
	float StrategicSample = 0.0f;
	if (WorldMapHudWidget && FParse::Value(FCommandLine::Get(), TEXT("RokStrategicAnimationSample="), StrategicSample))
	{
		if (const auto* Camera = Cast<ARokWorldMapCameraPawn>(GetPawn()))
		{
			const float Dxf = Camera->GetCurrentDxf();
			WorldMapHudWidget->UpdateZoomPresentation(Dxf > 5400 ? 3500.0f : 6000.0f);
			WorldMapHudWidget->AdvanceStrategicModuleAnimation(0.25f);
			WorldMapHudWidget->UpdateZoomPresentation(Dxf);
			WorldMapHudWidget->AdvanceStrategicModuleAnimation(FMath::Clamp(StrategicSample, 0.0f, 0.25f));
		}
	}
	int32 PressIndex = -1;
	if (WorldMapHudWidget && FParse::Value(FCommandLine::Get(), TEXT("RokFilterPressIndex="), PressIndex)
		&& PressIndex >= 0 && PressIndex < 4)
	{
		if (auto* Button = Cast<UButton>(WorldMapHudWidget->GetWidgetFromName(
			FName(*FString::Printf(TEXT("WorldMapFilterHit_%d"), PressIndex)))))
		{
			TSet<FKey> Buttons;
			Buttons.Add(EKeys::LeftMouseButton);
			const FPointerEvent Pointer(0, FVector2D::ZeroVector, FVector2D::ZeroVector,
				Buttons, EKeys::LeftMouseButton, 0, FModifierKeysState());
			Button->TakeWidget()->OnMouseEnter(Button->GetCachedGeometry(), Pointer);
			Button->TakeWidget()->OnMouseButtonDown(Button->GetCachedGeometry(), Pointer);
			WorldMapHudWidget->AdvanceFilterInputFeedback(0.1f);
			UE_LOG(LogTemp, Display, TEXT("Rok filter tint audit: row=%d pressed=%d"), PressIndex, Button->IsPressed());
		}
	}
	int32 FilterIndex = -1;
	if (WorldMapHudWidget && FParse::Value(FCommandLine::Get(), TEXT("RokFilterClickIndex="), FilterIndex)
		&& FilterIndex >= 0 && FilterIndex < 4)
	{
		if (auto* Button = Cast<UButton>(WorldMapHudWidget->GetWidgetFromName(
			FName(*FString::Printf(TEXT("WorldMapFilterHit_%d"), FilterIndex)))))
		{
			Button->OnClicked.Broadcast();
			float Sample = 0.1f;
			FParse::Value(FCommandLine::Get(), TEXT("RokFilterAnimationSample="), Sample);
			WorldMapHudWidget->AdvanceFilterSelectionAnimation(Sample);
		}
	}
	if (WorldMapHudWidget && FParse::Value(FCommandLine::Get(), TEXT("RokMiniMapAnimationSample="), MiniMapSample))
	{
		if (const auto* Camera = Cast<ARokWorldMapCameraPawn>(GetPawn()))
		{
			const float Dxf = Camera->GetCurrentDxf();
			auto* Tactical = Cast<URokMainHudWidget>(WorldMapHudWidget->GetWidgetFromName(TEXT("WorldMapTacticalHud")));
			WorldMapHudWidget->UpdateZoomPresentation(Dxf > 2997 ? 1537.0f : 3500.0f);
			WorldMapHudWidget->AdvanceMiniMapAnimation(0.25f);
			if (Tactical) Tactical->AdvanceWorldMapModuleAnimation(0.25f);
			WorldMapHudWidget->UpdateZoomPresentation(Dxf);
			WorldMapHudWidget->AdvanceMiniMapAnimation(FMath::Clamp(MiniMapSample, 0.0f, 0.25f));
			if (Tactical) Tactical->AdvanceWorldMapModuleAnimation(FMath::Clamp(MiniMapSample, 0.0f, 0.25f));
		}
	}
#endif
	FString ScreenshotPath;
	if (!FParse::Value(
			FCommandLine::Get(),
			TEXT("RokScreenshotPath="),
			ScreenshotPath)
		|| ScreenshotPath.IsEmpty())
	{
		ScreenshotPath = FPaths::ProjectSavedDir()
			/ TEXT("RokWorldMap_AutoScreenshot.png");
	}

	IFileManager::Get().MakeDirectory(
		*FPaths::GetPath(ScreenshotPath),
		true);
	FPaths::NormalizeFilename(ScreenshotPath);
	FScreenshotRequest::RequestScreenshot(
		ScreenshotPath,
		true,
		false,
		false,
		FIntRect(),
		true);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map screenshot: requested UI-inclusive capture %s"),
		*ScreenshotPath);

	if (FParse::Param(
			FCommandLine::Get(),
			TEXT("RokScreenshotQuit")))
	{
		GetWorldTimerManager().SetTimer(
			AutoScreenshotQuitTimerHandle,
			this,
			&ARokWorldMapPlayerController::QuitAfterAutoScreenshot,
			5.0f,
			false);
	}
}

#if !UE_BUILD_SHIPPING
bool ARokWorldMapPlayerController::RunSelectionLodAudit()
{
	auto* Camera = Cast<ARokWorldMapCameraPawn>(GetPawn());
	if (!Camera || !WorldMapHudWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("Rok selection LOD audit FAILED: missing camera/HUD"));
		return false;
	}
	const auto Defer = [&]()
	{
		GetWorldTimerManager().SetTimer(AutoScreenshotTimerHandle, this,
			&ARokWorldMapPlayerController::TakeAutoScreenshot, 1.0f, false);
		return true;
	};
	if (SelectionLodAuditStage == 0)
	{
		int32 Width = 0, Height = 0;
		GetViewportSize(Width, Height);
		for (TActorIterator<ARokWorldMapEntityActor> It(GetWorld()); It; ++It)
		{
			const FString Kind = It->GetEntityKind();
			if (It->IsHidden() || !(Kind == TEXT("food") || Kind == TEXT("wood") || Kind == TEXT("stone") || Kind == TEXT("gold"))) continue;
			FVector2D Screen;
			if (!ProjectWorldLocationToScreen(It->GetSelectionAnchorWorldPosition(), Screen, true)
				|| Screen.X < 300 || Screen.X > Width-150 || Screen.Y < 200 || Screen.Y > Height-150) continue;
			const bool bSelected = SelectWorldMapActorAtScreenPosition(Screen);
			if (!bSelected || GetSelectedWorldMapActor() != *It || !WorldMapHudWidget->IsWorldObjectInfoOpen()) continue;
			SelectionLodAuditActor = *It;
			UE_LOG(LogTemp, Display, TEXT("Rok selection LOD audit NEAR PASS: source=%d dxf=%.1f screen=(%.1f,%.1f) info=open"), It->GetSourceId(), Camera->GetCurrentDxf(), Screen.X, Screen.Y);
			if (const auto* Popup = WorldMapHudWidget->GetWidgetFromName(TEXT("WorldObjectInfoRoot")))
			{
				UE_LOG(LogTemp, Display, TEXT("Rok popup animation begin: scale=%.3f"), Popup->GetRenderTransform().Scale.X);
			}
			SelectionLodAuditStage = 1;
			return Defer();
		}
		UE_LOG(LogTemp, Error, TEXT("Rok selection LOD audit FAILED: no resource selected through screen hit testing"));
		return false;
	}
	if (SelectionLodAuditStage == 1)
	{
		if (const auto* Popup = WorldMapHudWidget->GetWidgetFromName(TEXT("WorldObjectInfoRoot")))
		{
			const bool bSettled = FMath::IsNearlyEqual(Popup->GetRenderTransform().Scale.X, 1.0f) && WorldMapHudWidget->IsWorldObjectInfoOpen();
			if (!bSettled)
			{
				UE_LOG(LogTemp, Error, TEXT("Rok popup animation FAILED to settle"));
				return false;
			}
			UE_LOG(LogTemp, Display, TEXT("Rok popup animation OPEN PASS: scale=1"));
		}
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("WorldSelectionLodAuditNear.png"), true, false);
		SelectionLodAuditStage = 2;
		return Defer();
	}
	if (SelectionLodAuditStage == 2)
	{
		auto* Button = Cast<UButton>(WorldMapHudWidget->GetWidgetFromName(TEXT("WorldObjectInfoDetailsButton")));
		if (!Button) { UE_LOG(LogTemp, Error, TEXT("Resource page audit missing details button")); return false; }
		Button->OnClicked.Broadcast();
		SelectionLodAuditStage = 3;
		return Defer();
	}
	if (SelectionLodAuditStage == 3)
	{
		auto* Page = WorldMapHudWidget->GetWidgetFromName(TEXT("WorldObjectInfoDetailsRoot"));
		if (!Page || Page->GetVisibility() == ESlateVisibility::Collapsed || !FMath::IsNearlyEqual(Page->GetRenderOpacity(), 1.0f))
		{ UE_LOG(LogTemp, Error, TEXT("Resource page audit description failed")); return false; }
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("WorldResourceDescription.png"), true, false);
		UE_LOG(LogTemp, Display, TEXT("Resource page audit DESCRIPTION PASS"));
		SelectionLodAuditStage = 4;
		return Defer();
	}
	if (SelectionLodAuditStage == 4)
	{
		auto* Scroll = Cast<UScrollBox>(WorldMapHudWidget->GetWidgetFromName(TEXT("WorldObjectDescriptionScroll")));
		auto* Text = Cast<URichTextBlock>(WorldMapHudWidget->GetWidgetFromName(TEXT("WorldObjectInfoDetailsText")));
		if (!Scroll || !Text || !Text->GetText().ToString().Contains(TEXT("<b>25%</>")))
		{ UE_LOG(LogTemp, Error, TEXT("Resource rich text audit missing source emphasis")); return false; }
		Scroll->ScrollToEnd();
		SelectionLodAuditStage = 40;
		return Defer();
	}
	if (SelectionLodAuditStage == 40)
	{
		auto* Scroll = Cast<UScrollBox>(WorldMapHudWidget->GetWidgetFromName(TEXT("WorldObjectDescriptionScroll")));
		if (!Scroll || Scroll->GetScrollOffset() <= 0)
		{ UE_LOG(LogTemp, Error, TEXT("Resource rich text audit could not scroll")); return false; }
		UE_LOG(LogTemp, Display, TEXT("Resource rich text audit SCROLL PASS: offset=%.1f"), Scroll->GetScrollOffset());
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("WorldResourceDescriptionBottom.png"), true, false);
		SelectionLodAuditStage = 42;
		return Defer();
	}
	if (SelectionLodAuditStage == 42)
	{
		auto* Scroll = Cast<UScrollBox>(WorldMapHudWidget->GetWidgetFromName(TEXT("WorldObjectDescriptionScroll")));
		if (!Scroll) { UE_LOG(LogTemp, Error, TEXT("Resource rich text audit lost scroll box")); return false; }
		// Sample above the bottom as well: the source's emphasized 25% belongs
		// to paragraph three, preceding the final two paragraphs.
		Scroll->SetScrollOffset(Scroll->GetScrollOffset() * 0.75f);
		SelectionLodAuditStage = 43;
		return Defer();
	}
	if (SelectionLodAuditStage == 43)
	{
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("WorldResourceDescriptionEmphasis.png"), true, false);
		SelectionLodAuditStage = 41;
		return Defer();
	}
	if (SelectionLodAuditStage == 41)
	{
		auto* Button = Cast<UButton>(WorldMapHudWidget->GetWidgetFromName(TEXT("WorldObjectDescriptionBack")));
		if (!Button) { UE_LOG(LogTemp, Error, TEXT("Resource page audit missing back button")); return false; }
		Button->OnClicked.Broadcast();
		SelectionLodAuditStage = 5;
		return Defer();
	}
	if (SelectionLodAuditStage == 5)
	{
		auto* Page = WorldMapHudWidget->GetWidgetFromName(TEXT("WorldObjectInfoPanelContent"));
		if (!Page || Page->GetVisibility() == ESlateVisibility::Collapsed || !FMath::IsNearlyEqual(Page->GetRenderOpacity(), 1.0f))
		{ UE_LOG(LogTemp, Error, TEXT("Resource page audit return failed")); return false; }
		UE_LOG(LogTemp, Display, TEXT("Resource page audit RETURN PASS"));
		Camera->SetCurrentDxf(3500);
		SelectionLodAuditStage = 6;
		return Defer(); // Let normal controller/HUD ticks clear selection; do not clear it here.
	}
	bool bHighlight = false;
	bool bRejected = false;
	const bool bClearedByNormalTick = !GetSelectedWorldMapActor() && !WorldMapHudWidget->IsWorldObjectInfoOpen();
	if (auto* Actor = Cast<ARokWorldMapEntityActor>(SelectionLodAuditActor.Get()))
	{
		TArray<UPrimitiveComponent*> Components;
		Actor->GetComponents(Components);
		for (auto* Component : Components) bHighlight |= Component->bRenderCustomDepth;
		FVector2D Screen;
		if (ProjectWorldLocationToScreen(Actor->GetSelectionAnchorWorldPosition(), Screen, true))
			bRejected = !SelectWorldMapActorAtScreenPosition(Screen);
	}
	const bool bPassed = bClearedByNormalTick && !bHighlight && bRejected;
	if (bPassed)
	{
		UE_LOG(LogTemp, Display, TEXT("Rok selection LOD audit FAR PASS: selection=clear info=closed highlight=off click=rejected"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Rok selection LOD audit FAR FAILED: normal_tick_cleared=%d highlight=%d rejected=%d"), bClearedByNormalTick, bHighlight, bRejected);
	}
	return false;
}
#endif

void ARokWorldMapPlayerController::QuitAfterAutoScreenshot()
{
	ConsoleCommand(TEXT("quit"));
}

void ARokWorldMapPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(
			EKeys::LeftMouseButton,
			IE_Pressed,
			this,
			&ARokWorldMapPlayerController::HandlePrimaryClick);
		InputComponent->BindKey(
			EKeys::Home,
			IE_Pressed,
			this,
			&ARokWorldMapPlayerController::HandleFocusPlayerLocation);
	}
}

void ARokWorldMapPlayerController::HandleFocusPlayerLocation()
{
	FocusPlayerLocation();
}

ARokWorldMapCityActor*
ARokWorldMapPlayerController::FindPlayerCity() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ARokWorldMapCityActor> CityIterator(World);
		CityIterator;
		++CityIterator)
	{
		if (CityIterator->Tags.Contains(
				FRokWorldMapMath::WorldMapPlayerCityTag()))
		{
			return *CityIterator;
		}
	}
	return nullptr;
}

bool ARokWorldMapPlayerController::FocusPlayerLocation()
{
	ARokWorldMapCameraPawn* CameraPawn =
		Cast<ARokWorldMapCameraPawn>(GetPawn());
	ARokWorldMapCityActor* PlayerCity = FindPlayerCity();
	if (!CameraPawn || !PlayerCity)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world-map locate-self failed: camera_or_player_city_missing."));
		return false;
	}

	FRokWorldMapProjection Projection;
	if (UWorld* World = GetWorld())
	{
		TActorIterator<ARokWorldMapConfigActor> ConfigIterator(World);
		if (ConfigIterator)
		{
			Projection = ConfigIterator->Projection;
		}
	}
	const FVector2D PlayerUnity =
		Projection.UeToUnity(PlayerCity->GetActorLocation());
	const FVector2D PlayerDisplay =
		Projection.UnityToDisplay(PlayerUnity);
	CameraPawn->FocusDisplayCoordinate(PlayerDisplay);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map locate-self focused display=(%.2f,%.2f) actor=%s."),
		PlayerDisplay.X,
		PlayerDisplay.Y,
		*PlayerCity->GetActorNameOrLabel());
	return true;
}

bool ARokWorldMapPlayerController::IsWorldMapSelectableActor(const AActor* Actor)
{
	return Actor
		&& !Actor->IsHidden()
		&& FRokWorldMapMath::AreTagsWorldMapSelectable(Actor->Tags);
}

void ARokWorldMapPlayerController::HandlePrimaryClick()
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		SelectWorldMapActor(nullptr);
		return;
	}
	SelectWorldMapActorAtScreenPosition(FVector2D(MouseX, MouseY));
}

AActor* ARokWorldMapPlayerController::FindSelectableActorAtScreenPosition(
	FVector2D ScreenPosition) const
{
	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ZeroVector;
	if (!DeprojectScreenPositionToWorld(
			ScreenPosition.X,
			ScreenPosition.Y,
			RayOrigin,
			RayDirection))
	{
		return nullptr;
	}

	TArray<FHitResult> Hits;
	constexpr double SelectionTraceDistance = 10000000.0;
	const FVector RayEnd = RayOrigin + RayDirection * SelectionTraceDistance;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RokWorldMapSelection), true);
	if (!GetWorld())
	{
		return nullptr;
	}
	GetWorld()->LineTraceMultiByChannel(Hits, RayOrigin, RayEnd, ECC_Visibility, QueryParams);

	for (const FHitResult& Hit : Hits)
	{
		if (AActor* HitActor = Hit.GetActor(); IsWorldMapSelectableActor(HitActor))
		{
			return HitActor;
		}
	}

	// The migrated world entities are screen-facing billboards.  Their
	// rendered icon can be several pixels away from an axis-aligned 3D query
	// box after camera pitch and LOD scaling, so preserve the collision trace
	// as the first choice and then use a tight screen-space pick radius.
	AActor* ClosestProjectedActor = nullptr;
	constexpr float ProjectedPickRadiusPixels = 72.0f;
	float ClosestDistanceSquared =
		FMath::Square(ProjectedPickRadiusPixels);
	for (TActorIterator<ARokWorldMapEntityActor> EntityIterator(GetWorld());
		EntityIterator;
		++EntityIterator)
	{
		ARokWorldMapEntityActor* EntityActor = *EntityIterator;
		if (!IsWorldMapSelectableActor(EntityActor)
			|| EntityActor->GetCurrentLodBand()
				== ERokWorldMapEntityLodBand::Hidden)
		{
			continue;
		}
		FVector2D ProjectedPosition = FVector2D::ZeroVector;
		if (!ProjectWorldLocationToScreen(
				EntityActor->GetSelectionAnchorWorldPosition(),
				ProjectedPosition,
				true))
		{
			continue;
		}
		const float DistanceSquared = FVector2D::DistSquared(
			ScreenPosition,
			ProjectedPosition);
		if (DistanceSquared <= ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestProjectedActor = EntityActor;
		}
	}
	for (TActorIterator<ARokWorldMapCityActor> CityIterator(GetWorld());
		CityIterator;
		++CityIterator)
	{
		ARokWorldMapCityActor* CityActor = *CityIterator;
		if (!IsWorldMapSelectableActor(CityActor))
		{
			continue;
		}
		FVector2D ProjectedPosition = FVector2D::ZeroVector;
		if (!ProjectWorldLocationToScreen(
				CityActor->GetSelectionAnchorWorldPosition(),
				ProjectedPosition,
				true))
		{
			continue;
		}
		const float DistanceSquared = FVector2D::DistSquared(
			ScreenPosition,
			ProjectedPosition);
		if (DistanceSquared <= ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestProjectedActor = CityActor;
		}
	}
	if (ClosestProjectedActor
		&& FParse::Param(
			FCommandLine::Get(),
			TEXT("RokWorldMapDebugHud")))
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok world-map screen-space selection fallback: actor=%s distance=%.2fpx."),
			*ClosestProjectedActor->GetActorNameOrLabel(),
			FMath::Sqrt(ClosestDistanceSquared));
	}
	return ClosestProjectedActor;
}

bool ARokWorldMapPlayerController::SelectWorldMapActorAtScreenPosition(
	FVector2D ScreenPosition)
{
	if (const auto* Camera = Cast<ARokWorldMapCameraPawn>(GetPawn()))
	{
		if (!CanSelectWorldObjectAtDxf(Camera->GetCurrentDxf()))
		{
			ClearWorldMapSelection();
			return false;
		}
	}
	SelectWorldMapActor(
		FindSelectableActorAtScreenPosition(ScreenPosition));
	return SelectedWorldMapActor.IsValid();
}

void ARokWorldMapPlayerController::ClearWorldMapSelection()
{
	SelectWorldMapActor(nullptr);
}

bool ARokWorldMapPlayerController::RequestEnterPlayerCity(
	TSoftObjectPtr<UWorld> PlayerCityLevel,
	float EntryDxf)
{
	if (bPlayerCityEntryRequested
		|| PlayerCityLevel.IsNull()
		|| !FMath::IsFinite(EntryDxf)
		|| EntryDxf <= 0.0f)
	{
		return false;
	}

	bPlayerCityEntryRequested = true;
	const FString Options = FString::Printf(
		TEXT("RokCityEntryDxf=%.3f"),
		EntryDxf);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok player-city level travel requested: level=%s entry_dxf=%.3f."),
		*PlayerCityLevel.ToSoftObjectPath().ToString(),
		EntryDxf);
	UGameplayStatics::OpenLevelBySoftObjectPtr(
		this,
		PlayerCityLevel,
		true,
		Options);
	return true;
}

void ARokWorldMapPlayerController::SelectWorldMapActor(AActor* Actor)
{
	if (!IsWorldMapSelectableActor(Actor))
	{
		Actor = nullptr;
	}
	if (SelectedWorldMapActor.Get() == Actor)
	{
		if (WorldMapHudWidget)
		{
			WorldMapHudWidget->ShowWorldObjectInfo(Actor);
		}
		return;
	}

	ApplySelectionHighlight(SelectedWorldMapActor.Get(), false);
	SelectedWorldMapActor = Actor;
	ApplySelectionHighlight(Actor, true);
	if (WorldMapHudWidget)
	{
		WorldMapHudWidget->ShowWorldObjectInfo(Actor);
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map selection changed: actor=%s resource_info=%s."),
		Actor ? *Actor->GetActorNameOrLabel() : TEXT("<none>"),
		WorldMapHudWidget && WorldMapHudWidget->IsWorldObjectInfoOpen()
			? TEXT("open")
			: TEXT("closed"));
}

void ARokWorldMapPlayerController::ApplySelectionHighlight(AActor* Actor, bool bSelected) const
{
	if (!Actor)
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}
		PrimitiveComponent->SetRenderCustomDepth(bSelected);
		PrimitiveComponent->SetCustomDepthStencilValue(bSelected ? 3 : 0);
	}
}
