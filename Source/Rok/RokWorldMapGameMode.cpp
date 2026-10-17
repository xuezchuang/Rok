#include "RokWorldMapGameMode.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RokWorldMapCameraPawn.h"
#include "RokWorldMapCityActor.h"
#include "RokWorldMapConfigActor.h"
#include "RokWorldMapEntityActor.h"
#include "RokWorldMapEntitySubsystem.h"
#include "RokWorldMapHud.h"
#include "RokWorldMapPlayerController.h"
#include "RokWorldMapTypes.h"

ARokWorldMapGameMode::ARokWorldMapGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	DefaultPawnClass = ARokWorldMapCameraPawn::StaticClass();
	PlayerControllerClass = ARokWorldMapPlayerController::StaticClass();
	HUDClass = ARokWorldMapHud::StaticClass();
}

void ARokWorldMapGameMode::BeginPlay()
{
	Super::BeginPlay();
	SpawnWorldMapPopulation();
	NormalizeWorldMapCollision();

	const UWorld* World = GetWorld();
	const TCHAR* FlowCompletedOption =
		World
			? World->URL.GetOption(
				TEXT("RokMapFlowCompleted="),
				nullptr)
			: nullptr;
	const bool bFlowCompleted =
		FApp::IsUnattended()
		&& FlowCompletedOption
		&& FCString::Atoi(FlowCompletedOption) != 0;
	if (bFlowCompleted)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok map-flow runtime automation completed: final_level=%s."),
			World
				? *World->GetPackage()->GetName()
				: TEXT("<none>"));
		if (FParse::Param(
				FCommandLine::Get(),
				TEXT("RokMapFlowAutomationQuitOnComplete")))
		{
			AutomationFlowExitDeadlineSeconds =
				FPlatformTime::Seconds() + 0.5;
			SetActorTickEnabled(true);
		}
	}

	float CaptureDelaySeconds = 0.0f;
	if (!bFlowCompleted
		&& FApp::IsUnattended()
		&& FParse::Value(
			FCommandLine::Get(),
			TEXT("RokWorldMapAutomationCaptureDelaySeconds="),
			CaptureDelaySeconds)
		&& CaptureDelaySeconds > 0.0f)
	{
		AutomationCaptureDeadlineSeconds =
			FPlatformTime::Seconds()
			+ FMath::Max(static_cast<double>(CaptureDelaySeconds), 0.25);
		SetActorTickEnabled(true);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok world-map unattended screenshot scheduled after %.2f wall-clock seconds."),
			CaptureDelaySeconds);
	}

	float SelectionDelaySeconds = 0.0f;
	if (!bFlowCompleted
		&& FApp::IsUnattended()
		&& FParse::Value(
			FCommandLine::Get(),
			TEXT("RokWorldMapAutomationSelectCityDelaySeconds="),
			SelectionDelaySeconds)
		&& SelectionDelaySeconds > 0.0f)
	{
		AutomationSelectionDeadlineSeconds =
			FPlatformTime::Seconds()
			+ FMath::Max(static_cast<double>(SelectionDelaySeconds), 0.25);
		SetActorTickEnabled(true);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok world-map unattended city selection scheduled after %.2f wall-clock seconds."),
			SelectionDelaySeconds);
	}

	float CityEntryDelaySeconds = 0.0f;
	if (!bFlowCompleted
		&& FApp::IsUnattended()
		&& FParse::Value(
			FCommandLine::Get(),
			TEXT("RokWorldMapAutomationEnterCityDelaySeconds="),
			CityEntryDelaySeconds)
		&& CityEntryDelaySeconds > 0.0f)
	{
		AutomationCityEntryNextStepSeconds =
			FPlatformTime::Seconds()
			+ FMath::Max(
				static_cast<double>(CityEntryDelaySeconds),
				0.25);
		AutomationCityEntryStepCount = 0;
		SetActorTickEnabled(true);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok world-map unattended player-city entry scheduled after %.2f wall-clock seconds."),
			CityEntryDelaySeconds);
	}
}

void ARokWorldMapGameMode::SpawnWorldMapPopulation()
{
	RuntimeEntityActors.Reset();
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = GetGameInstance();
	URokWorldMapEntitySubsystem* EntitySubsystem =
		GameInstance
			? GameInstance->GetSubsystem<URokWorldMapEntitySubsystem>()
			: nullptr;
	if (!World || !EntitySubsystem)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world entity population spawn skipped: runtime owner unavailable."));
		return;
	}
	if (!EntitySubsystem->IsPopulationLoaded()
		&& !EntitySubsystem->LoadPopulation())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world entity population spawn skipped: manifest unavailable."));
		return;
	}

	ARokWorldMapConfigActor* RuntimeConfig = nullptr;
	TActorIterator<ARokWorldMapConfigActor> ConfigIterator(World);
	if (ConfigIterator)
	{
		RuntimeConfig = *ConfigIterator;
	}
	const FRokWorldMapProjection Projection =
		RuntimeConfig
			? RuntimeConfig->Projection
			: FRokWorldMapProjection();
	const float InitialDxf =
		RuntimeConfig ? RuntimeConfig->GetDefaultDxf() : 1509.0f;

	int32 FailedCount = 0;
	for (const FRokWorldMapEntityRecord& Record :
		EntitySubsystem->GetEntities())
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FVector SpawnLocation = Projection.UnityToUe(
			Record.UnityPosition);
		SpawnLocation.Z += 20.0f;
		ARokWorldMapEntityActor* EntityActor =
			World->SpawnActor<ARokWorldMapEntityActor>(
				ARokWorldMapEntityActor::StaticClass(),
				SpawnLocation,
				FRotator::ZeroRotator,
				SpawnParameters);
		if (!EntityActor
			|| !EntityActor->Configure(Record, InitialDxf))
		{
			if (EntityActor)
			{
				EntityActor->Destroy();
			}
			++FailedCount;
			continue;
		}
#if WITH_EDITOR
		EntityActor->SetActorLabel(FString::Printf(
			TEXT("RokWorld_%s_%d"),
			*Record.Kind,
			Record.SourceId));
		EntityActor->SetFolderPath(
			FName(TEXT("RokUnityWorldMap/RuntimeEntities")));
#endif
		RuntimeEntityActors.Add(EntityActor);
	}

	if (FailedCount == 0)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok world entity population spawned: requested=%d spawned=%d failed=%d initial_dxf=%.1f."),
			EntitySubsystem->GetEntityCount(),
			RuntimeEntityActors.Num(),
			FailedCount,
			InitialDxf);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("Rok world entity population spawned with failures: requested=%d spawned=%d failed=%d initial_dxf=%.1f."),
			EntitySubsystem->GetEntityCount(),
			RuntimeEntityActors.Num(),
			FailedCount,
			InitialDxf);
	}
}

bool ARokWorldMapGameMode::ApplyWorldMapCollisionPolicy(
	const TArray<FName>& ActorTags,
	UPrimitiveComponent* PrimitiveComponent)
{
	if (!PrimitiveComponent)
	{
		return false;
	}

	if (FRokWorldMapMath::ShouldDisableWorldMapCollision(ActorTags))
	{
		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
		return true;
	}

	if (FRokWorldMapMath::AreTagsWorldMapSelectable(ActorTags))
	{
		PrimitiveComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
		if (PrimitiveComponent->ComponentTags.Contains(
				FRokWorldMapMath::WorldMapSelectionColliderTag()))
		{
			PrimitiveComponent->SetCollisionEnabled(
				ECollisionEnabled::QueryOnly);
			PrimitiveComponent->SetCollisionResponseToChannel(
				ECC_Visibility,
				ECR_Block);
		}
		else
		{
			PrimitiveComponent->SetCollisionEnabled(
				ECollisionEnabled::NoCollision);
		}
		return true;
	}

	return false;
}

void ARokWorldMapGameMode::NormalizeWorldMapCollision()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 NonSelectableComponentCount = 0;
	int32 SelectableComponentCount = 0;
	for (TActorIterator<AActor> ActorIterator(World);
		ActorIterator;
		++ActorIterator)
	{
		AActor* Actor = *ActorIterator;
		if (!Actor)
		{
			continue;
		}

		const bool bDisableCollision =
			FRokWorldMapMath::ShouldDisableWorldMapCollision(Actor->Tags);
		const bool bSelectable =
			FRokWorldMapMath::AreTagsWorldMapSelectable(Actor->Tags);
		if (!bDisableCollision && !bSelectable)
		{
			continue;
		}

		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (ApplyWorldMapCollisionPolicy(
					Actor->Tags,
					PrimitiveComponent))
			{
				if (bDisableCollision)
				{
					++NonSelectableComponentCount;
				}
				else if (PrimitiveComponent->ComponentTags.Contains(
					FRokWorldMapMath::WorldMapSelectionColliderTag()))
				{
					++SelectableComponentCount;
				}
			}
		}
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map collision policy applied: nonselectable_components=%d selectable_components=%d."),
		NonSelectableComponentCount,
		SelectableComponentCount);
}

void ARokWorldMapGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double CurrentTimeSeconds = FPlatformTime::Seconds();
	if (AutomationSelectionDeadlineSeconds > 0.0
		&& CurrentTimeSeconds >= AutomationSelectionDeadlineSeconds)
	{
		AutomationSelectionDeadlineSeconds = 0.0;
		SelectAutomationWorldMapCity();
	}

	if (AutomationCaptureDeadlineSeconds > 0.0
		&& CurrentTimeSeconds >= AutomationCaptureDeadlineSeconds)
	{
		AutomationCaptureDeadlineSeconds = 0.0;
		CaptureDelayedAutomationScreenshot();
	}

	if (AutomationCityEntryNextStepSeconds > 0.0
		&& CurrentTimeSeconds >= AutomationCityEntryNextStepSeconds)
	{
		DriveAutomationPlayerCityEntry();
	}

	if (AutomationFlowExitDeadlineSeconds > 0.0
		&& CurrentTimeSeconds >= AutomationFlowExitDeadlineSeconds)
	{
		AutomationFlowExitDeadlineSeconds = 0.0;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok map-flow runtime automation requesting clean exit."));
		FPlatformMisc::RequestExit(false);
		return;
	}

	if (AutomationSelectionDeadlineSeconds <= 0.0
		&& AutomationCaptureDeadlineSeconds <= 0.0
		&& AutomationCityEntryNextStepSeconds <= 0.0
		&& AutomationFlowExitDeadlineSeconds <= 0.0)
	{
		SetActorTickEnabled(false);
	}
}

void ARokWorldMapGameMode::SelectAutomationWorldMapCity()
{
	UWorld* World = GetWorld();
	ARokWorldMapPlayerController* PlayerController =
		World
			? Cast<ARokWorldMapPlayerController>(
				World->GetFirstPlayerController())
			: nullptr;
	ARokWorldMapCityActor* PlayerCity = nullptr;
	if (World)
	{
		for (TActorIterator<ARokWorldMapCityActor> CityIterator(World);
			CityIterator;
			++CityIterator)
		{
			if (FRokWorldMapMath::AreTagsWorldMapSelectable(
					CityIterator->Tags))
			{
				PlayerCity = *CityIterator;
				break;
			}
		}
	}

	FVector2D ScreenPosition = FVector2D::ZeroVector;
	const bool bProjected =
		PlayerController
		&& PlayerCity
		&& PlayerController->ProjectWorldLocationToScreen(
			PlayerCity->GetVisualCenterWorldPosition(),
			ScreenPosition,
			true);
	const bool bSelected =
		bProjected
		&& PlayerController->SelectWorldMapActorAtScreenPosition(
			ScreenPosition);
	const FString SelectedName =
		PlayerController && PlayerController->GetSelectedWorldMapActor()
			? PlayerController->GetSelectedWorldMapActor()->GetActorNameOrLabel()
			: TEXT("<none>");
	UE_LOG(
		LogTemp,
		Display,
		TEXT("Rok world-map automation city selection: projected=%s selected=%s actor=%s screen=(%.2f,%.2f)."),
		bProjected ? TEXT("true") : TEXT("false"),
		bSelected ? TEXT("true") : TEXT("false"),
		*SelectedName,
		ScreenPosition.X,
		ScreenPosition.Y);
}

void ARokWorldMapGameMode::DriveAutomationPlayerCityEntry()
{
	UWorld* World = GetWorld();
	ARokWorldMapCameraPawn* CameraPawn =
		World
			? Cast<ARokWorldMapCameraPawn>(
				World->GetFirstPlayerController()
					? World->GetFirstPlayerController()->GetPawn()
					: nullptr)
			: nullptr;
	if (!CameraPawn)
	{
		AutomationCityEntryNextStepSeconds = 0.0;
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Rok world-map automation player-city entry: result=failed reason=no_camera_pawn."));
		return;
	}

	const float DxfBeforeZoom = CameraPawn->GetCurrentDxf();
	const bool bEntryStarted =
		CameraPawn->ApplyZoomInput(-1.0f, false);
	++AutomationCityEntryStepCount;
	if (bEntryStarted)
	{
		AutomationCityEntryNextStepSeconds = 0.0;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("Rok world-map automation player-city entry: result=started steps=%d current_dxf=%.3f."),
			AutomationCityEntryStepCount,
			DxfBeforeZoom);
		return;
	}

	const float DxfAfterZoom = CameraPawn->GetCurrentDxf();
	if (AutomationCityEntryStepCount >= 64
		|| DxfAfterZoom >= DxfBeforeZoom - UE_SMALL_NUMBER)
	{
		AutomationCityEntryNextStepSeconds = 0.0;
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Rok world-map automation player-city entry: result=failed reason=zoom_stalled steps=%d before_dxf=%.3f after_dxf=%.3f."),
			AutomationCityEntryStepCount,
			DxfBeforeZoom,
			DxfAfterZoom);
		return;
	}

	AutomationCityEntryNextStepSeconds =
		FPlatformTime::Seconds() + 0.05;
}

void ARokWorldMapGameMode::CaptureDelayedAutomationScreenshot()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController =
				World->GetFirstPlayerController())
		{
			const bool bCaptureUi = FParse::Param(
				FCommandLine::Get(),
				TEXT("RokWorldMapAutomationCaptureUI"));
			PlayerController->ConsoleCommand(
				bCaptureUi
					? TEXT("Shot showui")
					: TEXT("HighResShot 1"),
				true);
			UE_LOG(
				LogTemp,
				Display,
				TEXT("Rok world-map delayed runtime screenshot requested: ui=%s."),
				bCaptureUi ? TEXT("true") : TEXT("false"));
		}
	}
}
