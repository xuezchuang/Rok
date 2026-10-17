#pragma once

#include "CoreMinimal.h"

// Local, base-rate simulation of BuildingResourcesProxy's BuildingGainInfo clock.
// Source account attribute multipliers and server-authoritative settlement are external.
struct FRokCityResourceProductionInput
{
	bool bCompleted = false;
	int32 ProducePerHour = 0;
	int32 GatherMin = 0;
	int32 GatherMax = 0;
	int64 StoredNum = 0;
	int64 ChangeTimeSeconds = 0;
	int64 NowSeconds = 0;
};

struct FRokCityResourceProductionResult
{
	// Source HUD uses an unrounded quantity for the gather threshold.
	double AccruedAmount = 0.0;
	// Source GetCurrencyNum floors this quantity for the collection credit.
	int64 Accrued = 0;
	bool bCanCollect = false;
	bool bAtCapacity = false;
};

class FRokCityResourceProduction
{
public:
	// Uses floor(StoredNum + elapsedSeconds * ProducePerHour / 3600), capped at GatherMax.
	static bool Evaluate(const FRokCityResourceProductionInput& Input,
		FRokCityResourceProductionResult& OutResult, FString& OutError);

	// Produces a new local clock snapshot, dropping any fractional remainder. The
	// server owns the real BuildingGainInfo.num/changeTime post-collection values.
	static bool Collect(const FRokCityResourceProductionInput& Input,
		FRokCityResourceProductionResult& OutCollected,
		FRokCityResourceProductionInput& OutNextState, FString& OutError);
};
