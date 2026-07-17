#include "RokPlayerController.h"

#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokBuildingActor.h"
#include "RokCitySpriteActor.h"
#include "RokMainHudWidget.h"
#include "StaticMeshResources.h"
#include "TimerManager.h"

namespace
{
bool IsPointInsideTriangle2D(const FVector2D& Point, const FVector2D& A, const FVector2D& B, const FVector2D& C)
{
	const auto SignedArea = [](const FVector2D& P1, const FVector2D& P2, const FVector2D& P3)
	{
		return (P1.X - P3.X) * (P2.Y - P3.Y) - (P2.X - P3.X) * (P1.Y - P3.Y);
	};

	const double TriangleArea = SignedArea(A, B, C);
	if (FMath::IsNearlyZero(TriangleArea, 0.01))
	{
		return false;
	}

	const double D1 = SignedArea(Point, A, B);
	const double D2 = SignedArea(Point, B, C);
	const double D3 = SignedArea(Point, C, A);
	const bool bHasNegative = D1 < -0.01 || D2 < -0.01 || D3 < -0.01;
	const bool bHasPositive = D1 > 0.01 || D2 > 0.01 || D3 > 0.01;
	return !(bHasNegative && bHasPositive);
}

bool IsPointInsideProjectedTightMesh(
	const APlayerController& PlayerController,
	const UStaticMeshComponent& MeshComponent,
	const FVector2D& ScreenPoint)
{
	const UStaticMesh* StaticMesh = MeshComponent.GetStaticMesh();
	const FStaticMeshRenderData* RenderData = StaticMesh ? StaticMesh->GetRenderData() : nullptr;
	if (!RenderData || RenderData->LODResources.IsEmpty())
	{
		return false;
	}

	const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
	const FPositionVertexBuffer& PositionBuffer = Lod.VertexBuffers.PositionVertexBuffer;
	const FIndexArrayView Indices = Lod.IndexBuffer.GetArrayView();
	const FTransform ComponentTransform = MeshComponent.GetComponentTransform();
	for (int32 Index = 0; Index + 2 < Indices.Num(); Index += 3)
	{
		const uint32 VertexIndices[3] = { Indices[Index], Indices[Index + 1], Indices[Index + 2] };
		if (VertexIndices[0] >= PositionBuffer.GetNumVertices() ||
			VertexIndices[1] >= PositionBuffer.GetNumVertices() ||
			VertexIndices[2] >= PositionBuffer.GetNumVertices())
		{
			continue;
		}

		FVector2D ScreenVertices[3];
		bool bProjectedTriangle = true;
		for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
		{
			const FVector WorldPosition = ComponentTransform.TransformPosition(
				FVector(PositionBuffer.VertexPosition(VertexIndices[VertexIndex])));
			if (!PlayerController.ProjectWorldLocationToScreen(WorldPosition, ScreenVertices[VertexIndex], true))
			{
				bProjectedTriangle = false;
				break;
			}
		}

		if (bProjectedTriangle && IsPointInsideTriangle2D(
			ScreenPoint,
			ScreenVertices[0],
			ScreenVertices[1],
			ScreenVertices[2]))
		{
			return true;
		}
	}

	return false;
}
}

ARokPlayerController::ARokPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void ARokPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
	ScheduleAutoScreenshot();

	if (FParse::Param(FCommandLine::Get(), TEXT("RokNoHud")))
	{
		return;
	}

	if (!MainHudWidget)
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
	}
}

void ARokPlayerController::HandlePrimaryClick()
{
	AActor* HitActor = FindSelectableBuildingUnderCursor();
	if (!HitActor)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Rok selection: no building under cursor."));
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

	const FVector2D MousePosition(MouseX, MouseY);
	AActor* BestActor = nullptr;
	double BestCameraDistanceSquared = TNumericLimits<double>::Max();
	const FVector CameraLocation = PlayerCameraManager
		? PlayerCameraManager->GetCameraLocation()
		: FVector::ZeroVector;

	for (TActorIterator<AActor> ActorIt(GetWorld()); ActorIt; ++ActorIt)
	{
		ARokCitySpriteActor* Candidate = Cast<ARokCitySpriteActor>(*ActorIt);
		if (!Candidate || Candidate->IsHidden() || !IsSelectableBuilding(Candidate))
		{
			continue;
		}

		UStaticMeshComponent* TightSelectionMesh = nullptr;
		TInlineComponentArray<UStaticMeshComponent*> MeshComponents;
		Candidate->GetComponents(MeshComponents);
		for (UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			if (MeshComponent && MeshComponent->GetFName() == TEXT("SelectionTightMesh"))
			{
				TightSelectionMesh = MeshComponent;
				break;
			}
		}
		if (!TightSelectionMesh || !IsPointInsideProjectedTightMesh(*this, *TightSelectionMesh, MousePosition))
		{
			continue;
		}

		const double CameraDistanceSquared = FVector::DistSquared(CameraLocation, Candidate->GetActorLocation());
		if (CameraDistanceSquared < BestCameraDistanceSquared)
		{
			BestActor = Candidate;
			BestCameraDistanceSquared = CameraDistanceSquared;
		}
	}

	if (BestActor)
	{
		return BestActor;
	}

	FHitResult HitResult;
	if (GetHitResultUnderCursor(ECC_Visibility, false, HitResult) &&
		HitResult.GetActor() &&
		IsSelectableBuilding(HitResult.GetActor()))
	{
		UE_LOG(LogTemp, Log, TEXT("Rok selection: tight mesh missed; using visibility hit %s."),
			*HitResult.GetActor()->GetName());
		return HitResult.GetActor();
	}

	return nullptr;
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
		if (MainHudWidget)
		{
			MainHudWidget->SetSelectedBuilding(Actor);
		}
		return;
	}

	ApplySelectionHighlight(SelectedBuildingActor.Get(), false);
	SelectedBuildingActor = Actor;
	ApplySelectionHighlight(Actor, true);
	if (MainHudWidget)
	{
		MainHudWidget->SetSelectedBuilding(Actor);
	}
}

void ARokPlayerController::ClearSelection()
{
	ApplySelectionHighlight(SelectedBuildingActor.Get(), false);
	SelectedBuildingActor.Reset();
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
