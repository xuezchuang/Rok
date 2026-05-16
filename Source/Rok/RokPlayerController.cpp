#include "RokPlayerController.h"

#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RokBuildingActor.h"
#include "RokMainHudWidget.h"
#include "TimerManager.h"

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
	FHitResult HitResult;
	if (!GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
	{
		ClearSelection();
		return;
	}

	AActor* HitActor = HitResult.GetActor();
	if (!IsSelectableBuilding(HitActor))
	{
		ClearSelection();
		return;
	}

	SelectBuilding(HitActor);
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
