#pragma once

#include "CoreMinimal.h"

/** Unity CityGlobalMediator's integer tile coordinates, not world-space hit geometry. */
struct FRokCityConstructionFootprint
{
	FIntPoint Origin = FIntPoint::ZeroValue;
	FIntPoint Size = FIntPoint::ZeroValue;

	bool IsValid() const { return Size.X > 0 && Size.Y > 0; }
	bool Overlaps(const FRokCityConstructionFootprint& Other) const
	{
		return int64(Origin.X) < int64(Other.Origin.X) + Other.Size.X
			&& int64(Other.Origin.X) < int64(Origin.X) + Size.X
			&& int64(Origin.Y) < int64(Other.Origin.Y) + Other.Size.Y
			&& int64(Other.Origin.Y) < int64(Origin.Y) + Size.Y;
	}
};

struct FRokCityConstructionInitialOccupant
{
	int32 InitId = 0;
	int32 TypeId = 0;
	int32 Level = 0;
	FRokCityConstructionFootprint Footprint;
};

struct FRokCityConstructionAge
{
	int32 Age = 0;
	int32 TownHallLevel = 0;
	int32 Size = 0;
	TArray<uint8> EmptyCells;
};

/** Immutable source map. Local reservations are supplied per query by the caller. */
class ROK_API FRokCityConstructionGrid
{
public:
	bool Load(FString& OutError);
	bool IsLoaded() const { return Ages.Num() == 5 && InitialOccupancy.Num() == 26; }
	int32 GetAgeForTownHallLevel(int32 TownHallLevel) const;
	bool CheckFootprint(int32 Age, const FRokCityConstructionFootprint& Footprint,
		TConstArrayView<FRokCityConstructionFootprint> LocalFootprints,
		FString& OutError, TArray<int32>* OutCellIndices = nullptr) const;
	const TArray<FRokCityConstructionInitialOccupant>& GetInitialOccupancy() const { return InitialOccupancy; }
	static int32 SourceCellIndex(int32 Size, int32 X, int32 Y);

private:
	TMap<int32, FRokCityConstructionAge> Ages;
	TArray<FRokCityConstructionInitialOccupant> InitialOccupancy;
};

struct FRokLocalConstructionResources
{
	int32 Food = 0;
	int32 Wood = 0;
	int32 Stone = 0;
	int32 Gold = 0;

	bool CanAfford(const FRokLocalConstructionResources& Cost) const
	{
		return Food >= Cost.Food && Wood >= Cost.Wood && Stone >= Cost.Stone && Gold >= Cost.Gold;
	}
};

struct FRokLocalConstructionTicket
{
	uint64 Id = 0;
	int32 TypeId = 0;
	int32 Civilization = 0;
	int32 Age = 0;
	FRokCityConstructionFootprint Footprint;
	FRokLocalConstructionResources Cost;
	int32 Power = 0;
	double StartedAt = 0.0;
	double FinishesAt = 0.0;
	bool bCompleted = false;
};

/** Pure transaction used by the world subsystem after source/account/presentation validation. */
class ROK_API FRokLocalConstructionLedger
{
public:
	bool TryBegin(const FRokCityConstructionGrid& Grid, FRokLocalConstructionTicket Proposed,
		int32 ActiveUpgrades, int32 OwnedQueues, int32 ExistingBuildingCount, int32 BuildingCountLimit,
		FRokLocalConstructionResources& InOutResources,
		uint64& OutId, FString& OutError);
	void CompleteDue(double Now, TArray<FRokLocalConstructionTicket>& OutCompleted);
	int32 GetActiveCount() const;
	const TArray<FRokLocalConstructionTicket>& GetTickets() const { return Tickets; }
	bool Find(uint64 Id, FRokLocalConstructionTicket& OutTicket) const;

private:
	uint64 NextId = 1;
	TArray<FRokLocalConstructionTicket> Tickets;
};
