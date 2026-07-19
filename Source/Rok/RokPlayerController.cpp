#include "RokPlayerController.h"

#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokBuildingActor.h"
#include "RokBuildingSelectionIndicator.h"
#include "RokCityGridMath.h"
#include "RokCitySpriteActor.h"
#include "RokMainHudWidget.h"
#include "TimerManager.h"

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
	ScheduleAutoScreenshot();
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

	UE_LOG(LogTemp, Log, TEXT("Rok selection: Unity footprint grid debug mode=%d (0=off, 1=selected, 2=all)."),
		CVarRokDebugBuildingGrid.GetValueOnGameThread());
	AutoSelectBuildingFromCommandLine();
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
	FString ScreenshotPath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("RokScreenshotPath="), ScreenshotPath) || ScreenshotPath.IsEmpty())
	{
		ScreenshotPath = FPaths::ProjectSavedDir() / TEXT("RokCityRuntime_AutoScreenshot.png");
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ScreenshotPath), true);
	FString NormalizedScreenshotPath = ScreenshotPath;
	FPaths::NormalizeFilename(NormalizedScreenshotPath);
	ConsoleCommand(FString::Printf(TEXT("HighResShot 1600x950 filename=%s"), *NormalizedScreenshotPath));
	UE_LOG(LogTemp, Log, TEXT("Rok screenshot: requested HighResShot %s"), *NormalizedScreenshotPath);

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
	DrawBuildingGridDebug();
}

void ARokPlayerController::HandlePrimaryClick()
{
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	const bool bHasMousePosition = GetMousePosition(MouseX, MouseY);
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
	if (!Actor)
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
