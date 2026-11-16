#include "RokCityLifeSubsystem.h"

#include "EngineUtils.h"
#include "RokBuildingUpgradeSubsystem.h"
#include "RokCityCitizenActor.h"
#include "RokCityGridMath.h"
#include "RokCityPlacementWorldFrame.h"
#include "RokCitySpriteActor.h"
#include "RokServerArmySubsystem.h"
#include "RokServerCitySubsystem.h"
#include "RokUnityGameplayCatalogSubsystem.h"

namespace
{
constexpr int32 CityHallType = 1;
constexpr int32 BarracksType = 9;

struct FCityBuilding
{
	int32 Index = 0;
	int32 Type = 0;
	int32 Level = 0;
	FRokCityConstructionFootprint Footprint;
};
}

bool URokCityLifeSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE;
}

TStatId URokCityLifeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URokCityLifeSubsystem, STATGROUP_Tickables);
}

bool URokCityLifeSubsystem::TryGetBarracksIdleMarkerLocation(FVector& OutLocation) const
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (const auto& Entry : Soldiers)
	{
		const auto* Soldier = Entry.Get();
		if (!Soldier || Soldier->IsHidden() || Soldier->IsTraining()) continue;
		Sum += Soldier->GetActorLocation();
		++Count;
	}
	if (Count == 0) return false;
	OutLocation = Sum / Count;
	return true;
}

void URokCityLifeSubsystem::ClearPeople()
{
	for (const auto& Person : People)
	{
		if (Person.IsValid()) Person->Destroy();
	}
	People.Reset();
	Soldiers.Reset();
}

void URokCityLifeSubsystem::Deinitialize()
{
	ClearPeople();
	Super::Deinitialize();
}

void URokCityLifeSubsystem::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !World->GetFirstPlayerController()
		|| UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != TEXT("/Game/Maps/L_RokUnityGameAppCity"))
	{
		return;
	}
	RefreshDelay -= DeltaSeconds;
	if (RefreshDelay > 0) return;
	RefreshDelay = 0.5f;

	const UGameInstance* Instance = World->GetGameInstance();
	auto* BuildingStates = World->GetSubsystem<URokBuildingUpgradeSubsystem>();
	const auto* Army = Instance ? Instance->GetSubsystem<URokServerArmySubsystem>() : nullptr;
	const auto* City = World->GetSubsystem<URokServerCitySubsystem>();
	if (!BuildingStates || (BuildingStates->UsesServerCity() && (!City || !City->HasAppliedServerCity()))) return;

	FString Error;
	FRokCityPlacementWorldFrame Frame;
	if ((!Grid.IsLoaded() && !Grid.Load(Error)) || !FRokCityPlacementWorldFrame::ResolveForWorld(World, Frame, Error)) return;

	TArray<FCityBuilding> Buildings;
	if (City && City->HasAppliedServerCity())
	{
		for (const FRokServerBuilding& Row : City->GetAppliedBuildings())
		{
			const auto* Sprite = Cast<ARokCitySpriteActor>(City->FindBuildingBarAnchor(Row.Index));
			const auto* Initial = Grid.GetInitialOccupancy().FindByPredicate([&Row](const auto& Entry) { return Entry.InitId == Row.Index; });
			const FIntPoint Size = Sprite ? Sprite->GetFootprintSize() : Initial ? Initial->Footprint.Size : FIntPoint::ZeroValue;
			if (Size.X > 0 && Size.Y > 0) Buildings.Add({Row.Index, Row.Type, Row.Level, {Row.Pos, Size}});
		}
	}
	else
	{
		for (TActorIterator<ARokCitySpriteActor> It(World); It; ++It)
		{
			const int32 Type = It->GetUnityBuildingType();
			const FIntPoint Size = It->GetFootprintSize();
			if (Type <= 0 || Size.X <= 0 || Size.Y <= 0 || It->IsHidden()) continue;
			const auto State = BuildingStates->GetBuildingStateForActor(*It);
			Buildings.Add({0, Type, State.Level, {It->GetGameplayTile(), Size}});
		}
	}
	const auto* Hall = Buildings.FindByPredicate([](const auto& Row) { return Row.Type == CityHallType && Row.Level > 0; });
	if (!Hall) return;
	const auto* Barracks = Buildings.FindByPredicate([](const auto& Row) { return Row.Type == BarracksType && Row.Level > 0; });
	const int32 Age = Grid.GetAgeForTownHallLevel(Hall->Level);
	const FRokServerArmySnapshot ArmyState = Army && Barracks ? Army->GetSnapshot() : FRokServerArmySnapshot();
	const auto* Queue = Barracks ? ArmyState.TrainingQueues.FindByPredicate([Barracks](const auto& Row)
	{
		return Row.BuildingIndex == Barracks->Index;
	}) : nullptr;
	const bool bTraining = Queue && Queue->State == ERokServerTrainingState::Training;
	FString SoldierModel = TEXT("Arms1_1");
	if (Army && Barracks && Barracks->Index > 0)
	{
		for (int32 Tier = 5; Tier > 0; --Tier)
		{
			FRokUnityCatalogTroop Troop;
			if (Army->GetTrainingTroop(1, Tier, Troop) && Army->IsTrainingTierUnlocked(Troop.StudyId) && !Troop.Model.IsEmpty())
			{
				SoldierModel = Troop.Model;
				break;
			}
		}
	}

	uint32 Hash = HashCombine(GetTypeHash(Age), GetTypeHash(SoldierModel));
	TArray<FRokCityConstructionFootprint> Occupied;
	for (const FCityBuilding& Building : Buildings)
	{
		Occupied.Add(Building.Footprint);
		Hash = HashCombine(Hash, GetTypeHash(Building.Type));
		Hash = HashCombine(Hash, GetTypeHash(Building.Footprint.Origin));
		Hash = HashCombine(Hash, GetTypeHash(Building.Footprint.Size));
		Hash = HashCombine(Hash, GetTypeHash(Building.Level > 0));
	}
	if (!bHasLayout || Hash != LayoutHash)
	{
		ClearPeople();
		bHasLayout = true;
		LayoutHash = Hash;
		const FTransform& Container = Frame.GetCityContainerTransform();
		const auto WorldPosition = [&Container](const FVector2D& Tile)
		{
			const FVector Ground = Container.TransformPosition(FVector(
				Tile.X * FRokCityGridMath::UnityCellSize, Tile.Y * FRokCityGridMath::UnityCellSize, 0.01));
			return Ground + FVector(0, 0, 4.0);
		};
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// CityAgeSize.BuilderMaleNum / BuilderFemaleNum are each age + 1.
		const int32 Desired = 2 * (Age + 1);
		const int32 SearchSpan = 23;
		const int32 HallX = Hall->Footprint.Origin.X + Hall->Footprint.Size.X / 2;
		const int32 HallY = Hall->Footprint.Origin.Y + Hall->Footprint.Size.Y / 2;
		TArray<FRokCityConstructionFootprint> Routes;
		FRandomStream Random(17031);
		for (int32 Attempt = 0; Attempt < 160 && Routes.Num() < Desired; ++Attempt)
		{
			// Prefer a walk along the open side of the barracks for one resident.
			// The remaining routes are spread around the city hall.
			const bool bNearBarracks = Barracks && Routes.IsEmpty() && Attempt < 40;
			const FIntPoint Center = bNearBarracks
				? Barracks->Footprint.Origin + FIntPoint(-3, Barracks->Footprint.Size.Y - 2)
				: FIntPoint(HallX, HallY);
			const int32 Radius = bNearBarracks ? 2 : SearchSpan / 2;
			const FIntPoint Start = Center + FIntPoint(Random.RandRange(-Radius, Radius), Random.RandRange(-Radius, Radius));
			const int32 Length = bNearBarracks ? 3 : Random.RandRange(4, 7);
			const FIntPoint Step = Random.RandRange(0, 1) == 0 ? FIntPoint(1, 0) : FIntPoint(0, 1);
			const FIntPoint End = Start + Step * Length;
			const FRokCityConstructionFootprint Route{Start, FIntPoint(1, 1) + Step * Length};
			if (Routes.ContainsByPredicate([&Route](const auto& Other) { return Route.Overlaps(Other); })
				|| !Grid.CheckFootprint(Age, Route, Occupied, Error, nullptr, false)) continue;
			auto* Person = World->SpawnActor<ARokCityCitizenActor>(Params);
			if (!Person) break;
			const FString Model = Routes.Num() % 2 == 0 ? TEXT("NPC_03") : TEXT("NPC_04");
			const FVector2D A(Start.X, Start.Y);
			const FVector2D B(End.X, End.Y);
			const bool bReverse = Routes.Num() % 2 == 0;
			if (!Person->ConfigureWalker(Model, WorldPosition(bReverse ? B : A), WorldPosition(bReverse ? A : B), Container, Attempt))
			{
				Person->Destroy();
				break;
			}
			People.Add(Person);
			Routes.Add(Route);
		}
		if (Barracks)
		{
			// TownSearch.ConvertTileToLocal adds half a tile. Use the barracks
			// rows of CityGlobalMediator.m_trainSoldierPos / m_trainSoldierDirPos.
			const FVector2D Origin(Barracks->Footprint.Origin.X, Barracks->Footprint.Origin.Y);
			const FVector2D Offsets[] = { {2, 1}, {1, 2} };
			const FVector2D Directions[] = { {-4, 0}, {3, 3} };
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Offsets); ++Index)
			{
				auto* Soldier = World->SpawnActor<ARokCityCitizenActor>(Params);
				if (!Soldier) continue;
				if (!Soldier->ConfigureSoldier(SoldierModel, WorldPosition(Origin + Offsets[Index]), Directions[Index], Index))
				{
					Soldier->Destroy();
					continue;
				}
				People.Add(Soldier);
				Soldiers.Add(Soldier);
			}
		}
		UE_LOG(LogTemp, Display, TEXT("Rok city life: %d walkers, %d barracks soldiers (%s)."), Routes.Num(), Soldiers.Num(), *SoldierModel);
	}

	for (const auto& Soldier : Soldiers)
	{
		if (Soldier.IsValid()) Soldier->SetTraining(bTraining);
	}
}
