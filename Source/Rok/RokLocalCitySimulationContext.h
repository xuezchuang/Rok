#pragma once

#include "CoreMinimal.h"
#include "RokCityConstructionGrid.h"
#include "RokCityResourceProduction.h"

class UWorld;

/** The chosen source city visual variant is a local fixture, never a player civilization. */
class ROK_API FRokLocalCitySimulationContext
{
public:
	bool Load(FString& OutError);
	bool ValidateWorld(const UWorld* World, FString& OutError) const;
	bool IsLoaded() const { return bLoaded; }
	bool SupportsAge(int32 Age) const { return SupportedAges.Contains(Age); }
	int32 GetVisualCivilization() const { return VisualCivilization; }
	int32 GetInitialAge() const { return InitialAge; }
	int32 GetInitialHallLevel() const { return InitialHallLevel; }
	FIntPoint GetInitialHallTile() const { return InitialHallTile; }
	const FString& GetTargetLevel() const { return TargetLevel; }

private:
	bool bLoaded = false;
	FString TargetLevel;
	int32 VisualCivilization = 0;
	int32 InitialAge = 0;
	int32 InitialHallId = 0;
	int32 InitialHallLevel = 0;
	FIntPoint InitialHallTile = FIntPoint::ZeroValue;
	TSet<int32> SupportedAges;
};

/** A local BuildingGainInfo clock, keyed by a completed and bound construction ticket. */
struct FRokLocalProductionSource
{
	int32 TypeId = 0;
	int32 Level = 0;
	int32 ProducePerHour = 0;
	int32 GatherMin = 0;
	int32 GatherMax = 0;
};

class ROK_API FRokLocalProductionLedger
{
public:
	bool RegisterCompleted(const FRokLocalConstructionTicket& Ticket,
		const FRokLocalProductionSource& Source, int64 CompletionSeconds, FString& OutError);
	bool GetSource(uint64 LocalConstructionId, FRokLocalProductionSource& OutSource, FString& OutError) const;
	// Unity re-evaluates retained BuildingGainInfo num/changeTime with the current level config.
	// This changes only the source row; it never credits the wallet or restarts the clock.
	bool TransitionLevel(uint64 LocalConstructionId, const FRokLocalProductionSource& ExpectedCurrent,
		const FRokLocalProductionSource& Next, FString& OutError);
	bool Preview(uint64 LocalConstructionId, int64 NowSeconds,
		FRokCityResourceProductionResult& OutResult, FString& OutError) const;
	bool TryCollect(uint64 LocalConstructionId, int64 NowSeconds,
		FRokLocalConstructionResources& InOutResources, int64& OutCollected, FString& OutError);
	bool HasClock(uint64 LocalConstructionId) const { return Clocks.Contains(LocalConstructionId); }

private:
	struct FClock
	{
		FRokLocalProductionSource Source;
		int64 StoredNum = 0;
		int64 ChangeTimeSeconds = 0;
	};
	TMap<uint64, FClock> Clocks;
};
