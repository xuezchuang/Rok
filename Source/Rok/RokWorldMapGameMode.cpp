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
#include "RokGameNetSubsystem.h"
#include "RokUnityConfigSubsystem.h"
#include "RokWorldMapArmyActor.h"
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
	const UGameInstance* Instance = GetGameInstance();
	const URokGameNetSubsystem* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (Net && Net->GetState() != ERokGameNetState::Offline)
	{
		StartServerPopulation();
	}
	else
	{
		SpawnWorldMapPopulation();
	}
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

void ARokWorldMapGameMode::StartServerPopulation()
{
	ServerPopulation.bServerMode = true;
	SetActorTickEnabled(true);
	TActorIterator<ARokWorldMapConfigActor> Config(GetWorld());
	if (Config)
	{
		ServerMapConfig = *Config;
	}
	for (TActorIterator<ARokWorldMapCityActor> City(GetWorld()); City; ++City)
	{
		if (City->ActorHasTag(FRokWorldMapMath::WorldMapPlayerCityTag()))
		{
			PlayerCityActor = *City;
			City->SetActorHiddenInGame(true);
			City->SetActorEnableCollision(false);
			break;
		}
	}
}

void ARokWorldMapGameMode::ClearServerPopulation()
{
	for (const auto& Pair : ServerEntityActors)
	{
		if (IsValid(Pair.Value)) Pair.Value->Destroy();
	}
	ServerEntityActors.Reset();
	for (const auto& Pair : ServerArmyActors)
		if (IsValid(Pair.Value)) Pair.Value->Destroy();
	ServerArmyActors.Reset();
	ServerPopulation.bRoleReady = false;
	ServerPopulation.ActiveObjects = 0;
	ServerPopulation.EntityActors = 0;
	ServerPopulation.OwnCityObjects = 0;
	ServerPopulation.UnmappedTypes.Reset();
	if (ARokWorldMapCityActor* City = PlayerCityActor.Get())
	{
		City->SetActorHiddenInGame(true);
		City->SetActorEnableCollision(false);
	}
	LastAoiGeneration = MAX_uint64;
	LastAoiRevision = MAX_uint64;
}

bool ARokWorldMapGameMode::HasVisibleArmyFormation(int64 ObjectId) const
{
	const auto* Army = ServerArmyActors.FindRef(ObjectId).Get();
	return IsValid(Army) && Army->IsFormationVisible();
}

void ARokWorldMapGameMode::ApplyServerHomePosition(const URokGameNetSubsystem& Net)
{
	ARokWorldMapCityActor* City = PlayerCityActor.Get();
	const TSharedPtr<FJsonObject> RoleInfo = Net.GetRoleInfo();
	const TSharedPtr<FJsonObject>* Pos = nullptr;
	int64 X = 0, Y = 0;
	if (!City || !RoleInfo.IsValid() || !RoleInfo->TryGetObjectField(TEXT("pos"), Pos)
		|| !RokSprotoJson::TryGetInt64(*Pos, TEXT("x"), X)
		|| !RokSprotoJson::TryGetInt64(*Pos, TEXT("y"), Y))
	{
		return;
	}
	const FRokWorldMapProjection Projection = ServerMapConfig.IsValid()
		? ServerMapConfig->Projection : FRokWorldMapProjection();
	const FVector2D HomeUnity(double(X) / 100.0, double(Y) / 100.0);
	if (!Projection.ClampUnity(HomeUnity).Equals(HomeUnity)) return;
	FVector Location = Projection.UnityToUe(HomeUnity);
	Location.Z = City->GetActorLocation().Z;
	if (!City->GetActorLocation().Equals(Location)) City->SetActorLocation(Location);
	City->SetActorHiddenInGame(false);
	City->SetActorEnableCollision(true);

	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	ARokWorldMapCameraPawn* Camera = Controller ? Cast<ARokWorldMapCameraPawn>(Controller->GetPawn()) : nullptr;
	if (!Camera) return;
	int64 Country = 0, HallLevel = 0;
	auto* Config = GetGameInstance()->GetSubsystem<URokUnityConfigSubsystem>();
	int32 Age = 0, AgeSize = 0, MaxSize = 0;
	FString ModelId;
	if (Config && RokSprotoJson::TryGetInt64(RoleInfo, TEXT("country"), Country) && Country >= 100 && Country <= 999
		&& RokSprotoJson::TryGetInt64(RoleInfo, TEXT("level"), HallLevel) && HallLevel >= 1 && HallLevel <= MAX_int32
		&& Config->GetIntFieldByKey(TEXT("BuildingTownCenter"), TEXT("ID"), int32(HallLevel), TEXT("age"), Age)
		&& Config->GetIntFieldByKey(TEXT("CityAgeSize"), TEXT("age"), Age, TEXT("size"), AgeSize)
		&& Config->GetIntFieldByKey(TEXT("CityAgeSize"), TEXT("age"), 5, TEXT("size"), MaxSize)
		&& AgeSize > 0 && MaxSize > 0)
	{
		// Unity BuildingModelData ID is the building type, civilization, and age.
		Config->GetStringFieldByKey(TEXT("BuildingModelData"), TEXT("ID"), 10000 + int32(Country) * 10 + Age,
			TEXT("modelId"), ModelId);
		City->ApplyHomeModel(ModelId, 7.8f * AgeSize / MaxSize, Projection.GetSafeUeScale(), Camera->GetCurrentDxf());
	}

	if (!bServerHomePositionApplied)
	{
		const bool bHasTravelFocus = GetWorld()->URL.GetOption(TEXT("RokWorldEntryX="), nullptr)
			&& GetWorld()->URL.GetOption(TEXT("RokWorldEntryY="), nullptr);
		float FocusX = -1.0f, FocusY = -1.0f;
		FParse::Value(FCommandLine::Get(), TEXT("RokWorldMapFocusX="), FocusX);
		FParse::Value(FCommandLine::Get(), TEXT("RokWorldMapFocusY="), FocusY);
		if (!bHasTravelFocus && !(FocusX >= 0.0f && FocusY >= 0.0f))
		{
			Camera->SetViewCenterUnity(HomeUnity);
		}
		bServerHomePositionApplied = true;
	}
}

void ARokWorldMapGameMode::UpdateServerPopulation()
{
	UGameInstance* Instance = GetGameInstance();
	URokGameNetSubsystem* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || Net->GetState() != ERokGameNetState::RoleReady)
	{
		if (ServerPopulation.bRoleReady) ClearServerPopulation();
		return;
	}
	if (LastAoiGeneration != Net->GetWorldMapAoiGeneration())
	{
		ClearServerPopulation();
		LastAoiGeneration = Net->GetWorldMapAoiGeneration();
	}
	ServerPopulation.bRoleReady = true;
	ApplyServerHomePosition(*Net);
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	const ARokWorldMapCameraPawn* Camera = Controller ? Cast<ARokWorldMapCameraPawn>(Controller->GetPawn()) : nullptr;
	if (Camera && bServerHomePositionApplied)
	{
		const FVector2D Center = Camera->GetViewCenterUnity();
		// GlobalViewLevelMediator remains tactical through the 5400 dxf transition.
		Net->RequestViewportAoi(FMath::FloorToInt64(Center.X), FMath::FloorToInt64(Center.Y),
			Camera->GetCurrentDxf() > 5400.0f);
	}
	if (LastAoiRevision == Net->GetWorldMapAoiRevision()) return;
	LastAoiRevision = Net->GetWorldMapAoiRevision();

	URokWorldMapEntitySubsystem* Entities = Instance->GetSubsystem<URokWorldMapEntitySubsystem>();
	const FRokWorldMapProjection Projection = ServerMapConfig.IsValid()
		? ServerMapConfig->Projection : FRokWorldMapProjection();
	const float Dxf = Camera ? Camera->GetCurrentDxf() : 1509.0f;
	const TSharedPtr<FJsonObject> Objects = Net->GetWorldMapObjects();
	int64 RoleId = Net->GetRoleId();
	if (RoleId <= 0) RokSprotoJson::TryGetInt64(Net->GetRoleInfo(), TEXT("rid"), RoleId);
	ServerPopulation.ActiveObjects = Objects.IsValid() ? Objects->Values.Num() : 0;
	ServerPopulation.OwnCityObjects = 0;
	ServerPopulation.UnmappedTypes.Reset();
	TSet<int64> Retained;
	TSet<int64> RetainedArmies;
	if (Objects.IsValid())
	{
		for (const auto& Pair : Objects->Values)
		{
			const TSharedPtr<FJsonObject>* Row = nullptr;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Row) || !Row) continue;
			int64 Type = 0, CityOwner = 0;
			RokSprotoJson::TryGetInt64(*Row, TEXT("objectType"), Type);
			int64 ArmyOwner = 0, ArmyId = 0;
			// RoleType.ARMY and Map_ObjectInfo are the map authority for displayed troops.
			if (Type == 1 && RoleId > 0 && RokSprotoJson::TryGetInt64(*Row, TEXT("armyRid"), ArmyOwner)
				&& ArmyOwner == RoleId && RokSprotoJson::TryGetInt64(*Row, TEXT("objectId"), ArmyId) && ArmyId > 0)
			{
				auto* Army = ServerArmyActors.FindRef(ArmyId).Get();
				if (!IsValid(Army))
				{
					Army = GetWorld()->SpawnActor<ARokWorldMapArmyActor>();
					if (Army) ServerArmyActors.Add(ArmyId, Army);
				}
				if (Army && Army->UpdateFromServer(*Row, Projection, *Net)) RetainedArmies.Add(ArmyId);
				continue;
			}
			if (Type == 3 && RoleId > 0 && RokSprotoJson::TryGetInt64(*Row, TEXT("cityRid"), CityOwner)
				&& CityOwner == RoleId && PlayerCityActor.IsValid())
			{
				++ServerPopulation.OwnCityObjects;
				continue; // RoleInfo owns the persistent home anchor even outside the AOI.
			}
			FRokWorldMapEntityRecord Record;
			if (!Entities || !Entities->ResolveServerEntity(*Row, Record)
				|| !Projection.ClampUnity(Record.UnityPosition).Equals(Record.UnityPosition))
			{
				++ServerPopulation.UnmappedTypes.FindOrAdd(int32(Type));
				continue;
			}
			FVector Location = Projection.UnityToUe(Record.UnityPosition);
			Location.Z += 20.0;
			ARokWorldMapEntityActor* Actor = ServerEntityActors.FindRef(Record.ServerObjectId);
			if (IsValid(Actor) && !Actor->ApplyServerUpdate(Record, Location))
			{
				Actor->Destroy();
				ServerEntityActors.Remove(Record.ServerObjectId);
				Actor = nullptr;
			}
			if (!IsValid(Actor))
			{
				FActorSpawnParameters Spawn;
				Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Actor = GetWorld()->SpawnActor<ARokWorldMapEntityActor>(Location, FRotator::ZeroRotator, Spawn);
				if (!Actor || !Actor->Configure(Record, Dxf))
				{
					if (Actor) Actor->Destroy();
					++ServerPopulation.UnmappedTypes.FindOrAdd(int32(Type));
					continue;
				}
#if WITH_EDITOR
				Actor->SetActorLabel(FString::Printf(TEXT("RokAOI_%s_%lld"), *Record.Kind, Record.ServerObjectId));
				Actor->SetFolderPath(FName(TEXT("RokUnityWorldMap/ServerAOI")));
#endif
				ServerEntityActors.Add(Record.ServerObjectId, Actor);
			}
			Retained.Add(Record.ServerObjectId);
		}
	}
	for (auto It = ServerEntityActors.CreateIterator(); It; ++It)
	{
		if (!Retained.Contains(It.Key()))
		{
			if (IsValid(It.Value())) It.Value()->Destroy();
			It.RemoveCurrent();
		}
	}
	for (auto It = ServerArmyActors.CreateIterator(); It; ++It)
	{
		if (!RetainedArmies.Contains(It.Key()))
		{
			if (IsValid(It.Value())) It.Value()->Destroy();
			It.RemoveCurrent();
		}
	}
	ServerPopulation.EntityActors = ServerEntityActors.Num();
	TArray<int32> Types;
	ServerPopulation.UnmappedTypes.GetKeys(Types);
	Types.Sort();
	TArray<FString> Unmapped;
	for (int32 Type : Types)
	{
		Unmapped.Add(FString::Printf(TEXT("%d:%d"), Type, ServerPopulation.UnmappedTypes.FindRef(Type)));
	}
	UE_LOG(LogTemp, Display,
		TEXT("Rok world AOI applied: revision=%llu active=%d actors=%d own_city=%d unmapped=%s."),
		LastAoiRevision, ServerPopulation.ActiveObjects, ServerPopulation.EntityActors,
		ServerPopulation.OwnCityObjects, *FString::Join(Unmapped, TEXT(",")));
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
	if (ServerPopulation.bServerMode)
	{
		ServerPopulationElapsed += DeltaSeconds;
		if (ServerPopulationElapsed >= 0.25f)
		{
			ServerPopulationElapsed = 0.0f;
			UpdateServerPopulation();
		}
	}
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

	if (!ServerPopulation.bServerMode
		&& AutomationSelectionDeadlineSeconds <= 0.0
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
