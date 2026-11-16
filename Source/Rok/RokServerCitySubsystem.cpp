#include "RokServerCitySubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "RokAudioRouterSubsystem.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokCityConstructionGrid.h"
#include "RokCitySpriteActor.h"
#include "RokGameNetSubsystem.h"
#include "RokLocalConstructionVisualFactory.h"
#include "RokSproto.h"
#include "RokUnityGameplayCatalogSubsystem.h"

namespace
{
constexpr double PollIntervalSeconds = 0.5;
constexpr int32 ConstructionStartSfx = 4025;
constexpr int32 ConstructionCompleteSfx = 4080;

FName InitTag(int32 Index)
{
	return FName(*FString::Printf(TEXT("UnityInitId_%d"), Index));
}

bool FootprintCenter(const FRokCityPlacementWorldFrame& Frame, const FIntPoint& Origin,
	const FIntPoint& Size, FVector& OutCenter, FString& OutError)
{
	return Frame.GridCenterToWorld(FRokCityConstructionFootprint{Origin, Size}, OutCenter, OutError);
}

void ReplaceIntTag(AActor& Actor, const TCHAR* Prefix, int32 Value)
{
	Actor.Tags.RemoveAll([Prefix](const FName& Tag) { return Tag.ToString().StartsWith(Prefix); });
	Actor.Tags.AddUnique(FName(*FString::Printf(TEXT("%s%d"), Prefix, Value)));
}

int32 GetCivilizationId(const URokGameNetSubsystem* Net)
{
	int64 Country = 0;
	return Net && RokSprotoJson::TryGetInt64(Net->GetRoleInfo(), TEXT("country"), Country)
		&& Country > 0 && Country <= MAX_int32 ? int32(Country) : 0;
}
}

namespace RokServerCity
{
bool ParseBuildings(const TSharedPtr<FJsonObject>& BuildingInfo, TArray<FRokServerBuilding>& Out,
	FString& OutError)
{
	Out.Reset();
	OutError.Reset();
	if (!BuildingInfo.IsValid())
	{
		OutError = TEXT("buildingInfo is missing");
		return false;
	}
	for (const auto& Pair : BuildingInfo->Values)
	{
		const TSharedPtr<FJsonObject>* Row = nullptr;
		if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Row) || !Row || !Row->IsValid())
		{
			OutError = FString::Printf(TEXT("buildingInfo[%s] is not an object"), *Pair.Key);
			return false;
		}
		int64 Index = 0, Type = 0, Level = 0, X = 0, Y = 0, FinishTime = -1;
		const TSharedPtr<FJsonObject>* Pos = nullptr;
		if (!RokSprotoJson::TryGetInt64(*Row, TEXT("buildingIndex"), Index) || Index <= 0
			|| !RokSprotoJson::TryGetInt64(*Row, TEXT("type"), Type) || Type <= 0
			|| !(*Row)->TryGetObjectField(TEXT("pos"), Pos) || !Pos
			|| !RokSprotoJson::TryGetInt64(*Pos, TEXT("x"), X)
			|| !RokSprotoJson::TryGetInt64(*Pos, TEXT("y"), Y))
		{
			OutError = FString::Printf(TEXT("buildingInfo[%s] lacks buildingIndex, type or pos"), *Pair.Key);
			return false;
		}
		RokSprotoJson::TryGetInt64(*Row, TEXT("level"), Level);
		RokSprotoJson::TryGetInt64(*Row, TEXT("finishTime"), FinishTime);
		FRokServerBuilding& Building = Out.AddDefaulted_GetRef();
		Building.Index = int32(Index);
		Building.Type = int32(Type);
		Building.Level = int32(Level);
		Building.Pos = FIntPoint(int32(X), int32(Y));
		Building.FinishTime = FinishTime;
	}
	Out.Sort([](const FRokServerBuilding& A, const FRokServerBuilding& B) { return A.Index < B.Index; });
	for (int32 I = 1; I < Out.Num(); ++I)
	{
		if (Out[I].Index == Out[I - 1].Index)
		{
			OutError = FString::Printf(TEXT("buildingIndex %d appears twice"), Out[I].Index);
			return false;
		}
	}
	return true;
}
}

bool URokServerCitySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

bool URokServerCitySubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE;
}

void URokServerCitySubsystem::Deinitialize()
{
	for (const auto& Pair : ConstructionAnchors)
	{
		if (AActor* Anchor = Pair.Value.Get()) Anchor->Destroy();
	}
	ConstructionAnchors.Reset();
	for (const auto& Pair : ConstructionBodies) if (ARokCitySpriteActor* Actor = Pair.Value.Get()) Actor->Destroy();
	ConstructionBodies.Reset();
	for (const auto& Pair : BoundaryActors) if (AActor* Actor = Pair.Value.Get()) Actor->Destroy();
	for (const auto& Pair : ConstructionScaffolds) if (AActor* Actor = Pair.Value.Get()) Actor->Destroy();
	BoundaryActors.Reset();
	ConstructionScaffolds.Reset();
	Structures.Reset();
	Particles.Reset();
	BuildingBarAnchors.Reset();
	SpawnedBuildings.Reset();
	AppliedBuildings.Reset();
	bFrameCached = false;
	Super::Deinitialize();
}

TStatId URokServerCitySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokServerCitySubsystem, STATGROUP_Tickables);
}

bool URokServerCitySubsystem::GetAppliedFrame(FRokCityPlacementWorldFrame& OutFrame) const
{
	if (!bFrameCached || AppliedBuildings.IsEmpty()) return false;
	OutFrame = Frame;
	return true;
}

ARokCitySpriteActor* URokServerCitySubsystem::FindSpawnedBuilding(int32 Index) const
{
	const TWeakObjectPtr<ARokCitySpriteActor>* Found = SpawnedBuildings.Find(Index);
	return Found ? Found->Get() : nullptr;
}

ARokCitySpriteActor* URokServerCitySubsystem::FindConstructionBody(int32 Index) const
{
	return ConstructionBodies.FindRef(Index).Get();
}

AActor* URokServerCitySubsystem::FindBuildingBarAnchor(int32 Index) const
{
	const TWeakObjectPtr<AActor>* Anchor = BuildingBarAnchors.Find(Index);
	return Anchor ? Anchor->Get() : nullptr;
}

const FRokServerBuilding* URokServerCitySubsystem::FindBuildingForActor(const AActor* Actor) const
{
	if (!IsValid(Actor) || Actor->GetWorld() != GetWorld()) return nullptr;
	return AppliedBuildings.FindByPredicate([this, Actor](const FRokServerBuilding& Row)
	{
		return Row.Level > 0 && FindBuildingBarAnchor(Row.Index) == Actor;
	});
}

FRokServerBuildQueueSnapshot URokServerCitySubsystem::GetBuildQueueSnapshot() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (QueueSnapshotOverride.IsSet()) return QueueSnapshotOverride.GetValue();
#endif
	const UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const URokGameNetSubsystem* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || Net->GetState() != ERokGameNetState::RoleReady) return {};
	TArray<FRokServerBuildQueue> Queues;
	FString Error;
	const bool bHasQueues = RokServerBuildQueue::Parse(Net->GetRoleInfo(), Queues, Error);
	double Now = 0.0;
	const bool bHasTime = Net->TryGetServerTimeSeconds(Now);
	FRokServerBuildQueueSnapshot Snapshot = RokServerBuildQueue::Project(Queues, bHasQueues, bHasTime, Now);
	const URokUnityGameplayCatalogSubsystem* Catalog = Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>();
	for (FRokServerBuildQueueView& Queue : Snapshot.Queues)
	{
		const FRokServerBuilding* Building = AppliedBuildings.FindByPredicate(
			[&Queue](const FRokServerBuilding& Row) { return Row.Index == Queue.BuildingIndex; });
		if (!Building) continue;
		Queue.bNewBuilding = Building->Level == 0;
		Queue.TargetLevel = Building->Level + 1;
		FRokUnityCatalogBuildingType Type;
		if (Catalog && Catalog->GetBuildingType(Building->Type, Type)) Queue.BuildingName = Type.DisplayNameZhCn;
	}
	return Snapshot;
}

void URokServerCitySubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (World)
	{
		Structures.Update(*World);
		Particles.Update(World->GetTimeSeconds());
	}
	if (!World || World->GetTimeSeconds() < NextPollSeconds) return;
	NextPollSeconds = World->GetTimeSeconds() + PollIntervalSeconds;

	const UGameInstance* Instance = World->GetGameInstance();
	const URokGameNetSubsystem* Net = Instance ? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr;
	if (!Net || Net->GetState() != ERokGameNetState::RoleReady) return;
	const FRokServerBuildQueueSnapshot Queues = GetBuildQueueSnapshot();
	FString Summary = FString::Printf(TEXT("clock=%d"), Queues.bTimeSynchronized);
	for (const FRokServerBuildQueueView& Queue : Queues.Queues)
	{
		Summary += FString::Printf(TEXT(" queue%d=%d building=%d"), Queue.QueueIndex, int32(Queue.State), Queue.BuildingIndex);
	}
	if (Summary != LastQueueSummary)
	{
		UE_LOG(LogTemp, Display, TEXT("Rok server build queues: %s source=%s"), *Summary,
			Net->IsSnapshotReplay() ? TEXT("snapshot_replay") : TEXT("live_server"));
		LastQueueSummary = Summary;
	}
	const TSharedPtr<FJsonObject> Info = Net->GetCollection(TEXT("Build_BuildingInfo"), TEXT("buildingInfo"));
	if (!Info.IsValid()) return;

	TArray<FRokServerBuilding> Buildings;
	FString Error;
	if (!RokServerCity::ParseBuildings(Info, Buildings, Error))
	{
		if (Error != LastFailure) UE_LOG(LogTemp, Warning, TEXT("Rok server city: %s"), *Error);
		LastFailure = Error;
		return;
	}
	if (Buildings == AppliedBuildings && GetCivilizationId(Net) == AppliedCivilizationId) return;
	if (!ApplyBuildings(Buildings, Error))
	{
		if (Error != LastFailure) UE_LOG(LogTemp, Warning, TEXT("Rok server city not applied: %s"), *Error);
		LastFailure = Error;
		return;
	}
	LastFailure.Reset();
}

bool URokServerCitySubsystem::CacheFrame(FString& OutError)
{
	if (bFrameCached) return true;
	// Must run before any source actor moves: ResolveForWorld checks every initial
	// actor against its source footprint.
	if (!FRokCityPlacementWorldFrame::ResolveForWorld(GetWorld(), Frame, OutError)) return false;
	FRokCityConstructionGrid Grid;
	if (!Grid.Load(OutError)) return false;
	InitialOrigins.Reset();
	InitialSizes.Reset();
	for (const FRokCityConstructionInitialOccupant& Initial : Grid.GetInitialOccupancy())
	{
		InitialOrigins.Add(Initial.InitId, Initial.Footprint.Origin);
		InitialSizes.Add(Initial.InitId, Initial.Footprint.Size);
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("UnitySourcePrefab_build_6_101_1"))
			&& It->ActorHasTag(TEXT("RokUnityRuntimeBoundarySource"))) MapWallActors.Add(*It);
		else if ((It->ActorHasTag(TEXT("UnitySourcePrefab_build_8_101_1"))
			&& It->ActorHasTag(TEXT("RokUnityRuntimeBoundarySource")))
			|| It->ActorHasTag(TEXT("RokUnityRuntimeCityGateFlag"))) MapTowerActors.Add(*It);
	}
	if (!Structures.Load())
		UE_LOG(LogTemp, Warning, TEXT("Rok city: Unity boundary and scaffold manifest could not load."));
	bFrameCached = true;
	return true;
}

AActor* URokServerCitySubsystem::ApplyBoundary(const FRokServerBuilding& Building, const FString& Model)
{
	AActor* Current = BoundaryActors.FindRef(Building.Index).Get();
	if (Current && Current->ActorHasTag(FName(*Model))) return Current;
	const FVector Center = Frame.GetCityContainerTransform().GetLocation();
	AActor* Next = Structures.Spawn(*GetWorld(), Model, true, Center);
	SetMapBoundaryVisible(Building.Type, Next == nullptr);
	if (Current) Current->Destroy();
	if (!Next)
	{
		BoundaryActors.Remove(Building.Index);
		return nullptr;
	}
	Next->Tags.Add(InitTag(Building.Index));
	Next->Tags.Add(FName(*FString::Printf(TEXT("UnityBuildingType_%d"), Building.Type)));
	Next->Tags.Add(Building.Type == 6 ? TEXT("RokServerCityWall") : TEXT("RokServerGuardTower"));
	BoundaryActors.Add(Building.Index, Next);
	return Next;
}

void URokServerCitySubsystem::SetMapBoundaryVisible(int32 Type, bool bVisible)
{
	const auto& MapActors = Type == 6 ? MapWallActors : MapTowerActors;
	for (const auto& Weak : MapActors)
	{
		if (AActor* Actor = Weak.Get())
		{
			Actor->SetActorHiddenInGame(!bVisible);
			Actor->SetActorEnableCollision(bVisible);
		}
	}
}

void URokServerCitySubsystem::UpdateConstructionScaffold(const FRokServerBuilding& Building, int32 Length,
	const FVector& Location, bool bHasShelf)
{
	AActor* Current = ConstructionScaffolds.FindRef(Building.Index).Get();
	if (!bHasShelf || Building.FinishTime <= 0)
	{
		if (Current) Current->Destroy();
		ConstructionScaffolds.Remove(Building.Index);
		return;
	}
	const FString Model = FString::Printf(TEXT("Build_shelf_%d"), Length);
	if (Current && Current->ActorHasTag(FName(*Model)))
	{
		Current->SetActorLocation(Location);
		return;
	}
	if (Current) Current->Destroy();
	ConstructionScaffolds.Remove(Building.Index);
	if (AActor* Scaffold = Structures.Spawn(*GetWorld(), Model, false, Location))
	{
		Scaffold->Tags.Add(FName(*FString::Printf(TEXT("RokServerConstructionIndex_%d"), Building.Index)));
		ConstructionScaffolds.Add(Building.Index, Scaffold);
	}
}

void URokServerCitySubsystem::ApplyConstructionEffects(const TArray<FRokServerBuilding>& Buildings,
	int32 Civilization, int32 HallLevel)
{
	if (AppliedBuildings.IsEmpty()) return;
	Particles.Load();
	for (const FRokServerBuilding& Previous : AppliedBuildings)
	{
		if (Previous.Level < 0) continue;
		const auto* Current = Buildings.FindByPredicate([&Previous](const FRokServerBuilding& Row)
		{
			return Row.Index == Previous.Index && Row.Type == Previous.Type;
		});
		if (!Current || Current->Level < 0 || (Previous.FinishTime > 0 && Current->FinishTime <= 0))
			Particles.Stop(Previous.Index);
	}
	// A civilization replacement can also refresh unrelated rows. Effects are
	// issued on actual building transitions within the same city.
	if (AppliedCivilizationId != Civilization) return;
	const auto* Upgrades = GetWorld()->GetSubsystem<URokBuildingUpgradeSubsystem>();
	auto* Audio = GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<URokAudioRouterSubsystem>() : nullptr;
	for (const FRokServerBuilding& Current : Buildings)
	{
		if (Current.Level < 0) continue;
		const auto* Previous = AppliedBuildings.FindByPredicate([&Current](const FRokServerBuilding& Row)
		{
			return Row.Index == Current.Index && Row.Type == Current.Type;
		});
		if (Previous && Previous->Level < 0) Previous = nullptr;
		const AActor* Anchor = FindBuildingBarAnchor(Current.Index);
		const auto* Scaffold = FindConstructionScaffold(Current.Index);
		if (Current.FinishTime > 0 && (!Previous || Previous->FinishTime <= 0))
		{
			const auto* Body = FindConstructionBody(Current.Index);
			if (Current.Level == 0 && Body)
				Particles.Play(*GetWorld(), Current.Index, TEXT("build_3003"), Body->GetVisualAnchorWorldPosition());
			if (Scaffold)
				Particles.Play(*GetWorld(), Current.Index, TEXT("build_3003_shelf"),
					Scaffold->GetActorLocation(), Scaffold->GetActorScale3D().X);
			if (Audio && Anchor && Current.Level == 0) Audio->PlaySourceSfx(ConstructionStartSfx);
		}
		else if (!Previous && Current.Level == 1 && Current.FinishTime == -1 && Anchor)
		{
			// Unity sends the same start event for buildables with no build time.
			if (const auto* Sprite = Cast<ARokCitySpriteActor>(Anchor))
			{
				Particles.Play(*GetWorld(), Current.Index, TEXT("build_3003"), Sprite->GetVisualAnchorWorldPosition());
				if (Audio) Audio->PlaySourceSfx(ConstructionStartSfx);
			}
		}
		else if (Current.Level > 0 && Current.FinishTime == -1 && Previous && Current.Level > Previous->Level)
		{
			if (!Anchor) continue;
			const auto* Sprite = Cast<ARokCitySpriteActor>(Anchor);
			const FString Model = Upgrades
				? Upgrades->GetCityConstructionModelId(Current.Type, HallLevel, Civilization) : FString();
			const FVector Location = Sprite ? Sprite->GetVisualAnchorWorldPosition() : Anchor->GetActorLocation();
			const FVector Offset = Frame.GetCityContainerTransform().TransformVector(
				Particles.GetCompletionOffset(FName(*Model)));
			Particles.Play(*GetWorld(), Current.Index, TEXT("build_3004"), Location + Offset);
			if (Audio) Audio->PlaySourceSfx(ConstructionCompleteSfx);
		}
	}
}

bool URokServerCitySubsystem::ApplyBuildings(const TArray<FRokServerBuilding>& Buildings, FString& OutError)
{
	OutError.Reset();
	UWorld* World = GetWorld();
	if (!World || !CacheFrame(OutError)) return false;
	URokBuildingUpgradeSubsystem* Upgrades = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	const UGameInstance* Instance = World->GetGameInstance();
	const URokUnityGameplayCatalogSubsystem* Catalog = Instance
		? Instance->GetSubsystem<URokUnityGameplayCatalogSubsystem>() : nullptr;
	const int32 CivilizationId = GetCivilizationId(Instance
		? Instance->GetSubsystem<URokGameNetSubsystem>() : nullptr);

	const FRokServerBuilding* Hall = Buildings.FindByPredicate([](const FRokServerBuilding& Building)
	{
		return Building.Type == 1 && Building.Level > 0;
	});
	const int32 HallLevel = Hall ? Hall->Level : 1;
	int32 Moved = 0, Leveled = 0, Spawned = 0, Skipped = 0;
	TSet<int32> ServerIndices;
	for (const FRokServerBuilding& Building : Buildings)
	{
		// Cancellation tombstones retain their keyed merged row, but no city actor.
		if (Building.Level < 0) continue;
		ServerIndices.Add(Building.Index);
		const FString ModelId = Upgrades ? Upgrades->GetCityConstructionModelId(Building.Type, HallLevel, CivilizationId) : FString();
		if (Building.Type == 6 || Building.Type == 8)
		{
			if (AActor* Boundary = ApplyBoundary(Building, ModelId))
			{
				BuildingBarAnchors.Add(Building.Index, Boundary);
				if (Upgrades) Upgrades->ApplyServerBuilding(Boundary, Building.Type, Building.Level, Building.Pos);
				++Leveled;
				continue;
			}
			BuildingBarAnchors.Remove(Building.Index);
		}
		if (Building.Level > 0)
		{
			if (TWeakObjectPtr<AActor>* Pending = ConstructionAnchors.Find(Building.Index))
			{
				if (AActor* Anchor = Pending->Get()) Anchor->Destroy();
				ConstructionAnchors.Remove(Building.Index);
			}
		}
		if (const FIntPoint* CurrentOrigin = InitialOrigins.Find(Building.Index))
		{
			TArray<AActor*> Actors;
			UGameplayStatics::GetAllActorsWithTag(World, InitTag(Building.Index), Actors);
			AActor* Body = nullptr;
			for (AActor* Actor : Actors)
			{
				if (Actor && Actor->ActorHasTag(TEXT("RokUnityConfigInitBuilding")))
				{
					Body = Actor;
					break;
				}
			}
			const FIntPoint Size = InitialSizes.FindRef(Building.Index);
			// Walls and towers (types 6 and 8) have no footprint: their pos is always 0,0.
			if (Size.X > 0 && Size.Y > 0 && Building.Pos != *CurrentOrigin)
			{
				FVector From, To;
				FString MoveError;
				if (!FootprintCenter(Frame, *CurrentOrigin, Size, From, MoveError)
					|| !FootprintCenter(Frame, Building.Pos, Size, To, MoveError))
				{
					OutError = FString::Printf(TEXT("building %d: %s"), Building.Index, *MoveError);
					return false;
				}
				const FVector Delta = To - From;
				for (AActor* Actor : Actors)
				{
					if (!Actor) continue;
					Actor->SetActorLocation(Actor->GetActorLocation() + Delta);
					if (ARokCitySpriteActor* Sprite = Cast<ARokCitySpriteActor>(Actor);
						Sprite && Sprite->GetGameplayTile() == *CurrentOrigin)
					{
						Sprite->SetGameplayTile(Building.Pos);
					}
					ReplaceIntTag(*Actor, TEXT("UnityGridX_"), Building.Pos.X);
					ReplaceIntTag(*Actor, TEXT("UnityGridY_"), Building.Pos.Y);
				}
				InitialOrigins[Building.Index] = Building.Pos;
				++Moved;
			}
			if (Body && Upgrades)
			{
				Upgrades->ApplyServerBuilding(Body, Building.Type, Building.Level, Building.Pos);
				if (Hall && CivilizationId > 0 && Size.X > 0 && Size.Y > 0 && Building.Level > 0)
				{
					if (ARokCitySpriteActor* Sprite = Cast<ARokCitySpriteActor>(Body))
					{
						FString VisualError;
						if (!FRokLocalConstructionVisualFactory::ApplyInitialBuildingVisual(
							*Sprite, Frame, Building.Type, ModelId, VisualError))
						{
							UE_LOG(LogTemp, Warning, TEXT("Rok server city: initial building %d visual: %s"),
								Building.Index, *VisualError);
						}
					}
				}
				BuildingBarAnchors.Add(Building.Index, Body);
				if (Size.X > 0 && Size.Y > 0)
				{
					FVector Center;
					if (!FootprintCenter(Frame, Building.Pos, Size, Center, OutError)) return false;
					UpdateConstructionScaffold(Building, Size.Y, Center,
						ModelId != TEXT("build_28_501_1") && !ModelId.IsEmpty());
				}
				++Leveled;
			}
			else if (!Body)
			{
				UE_LOG(LogTemp, Verbose, TEXT("Rok server city: initial building %d (type %d) has no UE actor."),
					Building.Index, Building.Type);
			}
			continue;
		}

		FRokUnityCatalogBuildingType TypeRow;
		if (!Catalog || !Catalog->GetBuildingType(Building.Type, TypeRow) || TypeRow.Width <= 0 || TypeRow.Length <= 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Rok server city: building %d type %d has no catalog footprint."),
				Building.Index, Building.Type);
			++Skipped;
			continue;
		}
		const FIntPoint Size(TypeRow.Width, TypeRow.Length);
		const FRokLocalSourceBuildingVisual* Visual = ModelId.IsEmpty() ? nullptr
			: FRokLocalConstructionVisualFactory::FindBuildingVisual(Building.Type, ModelId);
		if (!Visual && CivilizationId <= 0) Visual = FRokLocalConstructionVisualFactory::FindBuildingVisual(Building.Type);
		const FName ModelTag = Visual ? FName(*FString::Printf(TEXT("RokLocalSourceModel_%s"), Visual->ModelId)) : NAME_None;
		ARokCitySpriteActor* PendingBody = ConstructionBodies.FindRef(Building.Index).Get();
		if (PendingBody && (!Visual || !PendingBody->ActorHasTag(ModelTag)
			|| PendingBody->GetGameplayTile() != Building.Pos || (Building.Level == 0 && Building.FinishTime <= 0)))
		{
			PendingBody->Destroy();
			ConstructionBodies.Remove(Building.Index);
			PendingBody = nullptr;
		}
		if (Building.Level <= 0)
		{
			if (ARokCitySpriteActor* OldBody = FindSpawnedBuilding(Building.Index)) OldBody->Destroy();
			SpawnedBuildings.Remove(Building.Index);
			FVector Center;
			if (!FootprintCenter(Frame, Building.Pos, Size, Center, OutError)) return false;
			AActor* Anchor = ConstructionAnchors.FindRef(Building.Index).Get();
			if (!Anchor)
			{
				FActorSpawnParameters Params;
				Params.ObjectFlags |= RF_Transient;
				Anchor = World->SpawnActor<AActor>(AActor::StaticClass(), Params);
				if (!Anchor) { OutError = TEXT("Construction clock anchor could not spawn"); return false; }
				USceneComponent* Root = NewObject<USceneComponent>(Anchor, TEXT("ServerConstructionRoot"));
				Anchor->SetRootComponent(Root);
				Anchor->AddInstanceComponent(Root);
				Root->RegisterComponent();
				Anchor->Tags.Add(TEXT("RokServerConstructionAnchor"));
				ConstructionAnchors.Add(Building.Index, Anchor);
			}
			Anchor->SetActorLocation(Center);
			BuildingBarAnchors.Add(Building.Index, Anchor);
			if (!PendingBody && Visual && Building.FinishTime > 0)
			{
				FString SpawnError;
				PendingBody = FRokLocalConstructionVisualFactory::SpawnServerBuilding(World, Frame, *Visual,
					Building.Pos, Size, SpawnError);
				if (PendingBody)
				{
					PendingBody->Tags.Add(TEXT("RokServerConstructionBody"));
					PendingBody->Tags.Add(TEXT("RokLocalConstructionPending"));
					PendingBody->SetActorEnableCollision(false);
					ConstructionBodies.Add(Building.Index, PendingBody);
				}
			}
			UpdateConstructionScaffold(Building, Size.Y, Center, PendingBody && ModelId != TEXT("build_28_501_1"));
			continue;
		}
		ARokCitySpriteActor* Actor = FindSpawnedBuilding(Building.Index);
		if (Actor && (Actor->GetGameplayTile() != Building.Pos || !Visual
			|| !Actor->ActorHasTag(ModelTag)))
		{
			Actor->Destroy();
			Actor = nullptr;
		}
		if (!Actor)
		{
			FString SpawnError;
			if (PendingBody)
			{
				Actor = PendingBody;
				ConstructionBodies.Remove(Building.Index);
				Actor->Tags.Remove(TEXT("RokServerConstructionBody"));
				Actor->Tags.Remove(TEXT("RokLocalConstructionPending"));
				Actor->SetActorEnableCollision(true);
			}
			else if (Visual)
			{
				Actor = FRokLocalConstructionVisualFactory::SpawnServerBuilding(
					World, Frame, *Visual, Building.Pos, Size, SpawnError);
			}
			else
			{
				SpawnError = TEXT("No verified source visual");
			}
			if (!Actor)
			{
				UpdateConstructionScaffold(Building, Size.Y, FVector::ZeroVector, false);
				bool bAlreadyInSet = false;
				ReportedMissingVisuals.Add(Building.Type, &bAlreadyInSet);
				if (!bAlreadyInSet)
				{
					UE_LOG(LogTemp, Warning, TEXT("Rok server city: building %d type %d not shown: %s"),
						Building.Index, Building.Type, *SpawnError);
				}
				SpawnedBuildings.Remove(Building.Index);
				++Skipped;
				continue;
			}
#if WITH_EDITOR
			Actor->SetActorLabel(FString::Printf(TEXT("ServerBuilding_%d"), Building.Index));
#endif
			Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("RokServerBuildingIndex_%d"), Building.Index)));
			SpawnedBuildings.Add(Building.Index, Actor);
			++Spawned;
		}
		if (Upgrades)
		{
			Upgrades->ApplyServerBuilding(Actor, Building.Type, Building.Level, Building.Pos);
			++Leveled;
		}
		BuildingBarAnchors.Add(Building.Index, Actor);
		UpdateConstructionScaffold(Building, Size.Y, Actor->GetVisualAnchorWorldPosition(), ModelId != TEXT("build_28_501_1"));
		if (Building.FinishTime >= 0)
		{
			UE_LOG(LogTemp, Display, TEXT("Rok server city: building %d (type %d) is upgrading until %lld."),
				Building.Index, Building.Type, Building.FinishTime);
		}
	}

	for (auto It = SpawnedBuildings.CreateIterator(); It; ++It)
	{
		if (!ServerIndices.Contains(It.Key()))
		{
			if (ARokCitySpriteActor* Stale = It.Value().Get()) Stale->Destroy();
			It.RemoveCurrent();
		}
	}
	for (auto It = ConstructionAnchors.CreateIterator(); It; ++It)
	{
		if (!ServerIndices.Contains(It.Key()))
		{
			if (AActor* Anchor = It.Value().Get()) Anchor->Destroy();
			It.RemoveCurrent();
		}
	}
	for (auto It = ConstructionScaffolds.CreateIterator(); It; ++It)
	{
		if (!ServerIndices.Contains(It.Key()))
		{
			if (AActor* Scaffold = It.Value().Get()) Scaffold->Destroy();
			It.RemoveCurrent();
		}
	}
	for (auto It = ConstructionBodies.CreateIterator(); It; ++It)
	{
		if (ServerIndices.Contains(It.Key())) continue;
		if (ARokCitySpriteActor* Body = It.Value().Get()) Body->Destroy();
		It.RemoveCurrent();
	}
	for (auto It = BoundaryActors.CreateIterator(); It; ++It)
	{
		if (ServerIndices.Contains(It.Key())) continue;
		if (AActor* Boundary = It.Value().Get())
		{
			SetMapBoundaryVisible(Boundary->ActorHasTag(TEXT("RokServerCityWall")) ? 6 : 8, true);
			Boundary->Destroy();
		}
		It.RemoveCurrent();
	}
	for (auto It = BuildingBarAnchors.CreateIterator(); It; ++It)
	{
		if (!ServerIndices.Contains(It.Key()) || !It.Value().IsValid()) It.RemoveCurrent();
	}
	ApplyConstructionEffects(Buildings, CivilizationId, HallLevel);
	AppliedBuildings = Buildings;
	AppliedCivilizationId = CivilizationId;
	UE_LOG(LogTemp, Display,
		TEXT("Rok server city applied: buildings=%d moved=%d leveled=%d spawned=%d skipped=%d"),
		Buildings.Num(), Moved, Leveled, Spawned, Skipped);
	return true;
}
