#include "RokCityResourceProduction.h"

bool FRokCityResourceProduction::Evaluate(const FRokCityResourceProductionInput& Input,
	FRokCityResourceProductionResult& OutResult, FString& OutError)
{
	OutResult = {};
	OutError.Reset();
	if (Input.ProducePerHour <= 0 || Input.GatherMin <= 0 || Input.GatherMax < Input.GatherMin)
	{
		OutError = TEXT("Invalid BuildingResourcesProduce rate or gather bounds");
		return false;
	}
	if (Input.StoredNum < 0 || Input.ChangeTimeSeconds < 0 || Input.NowSeconds < 0)
	{
		OutError = TEXT("Negative BuildingGainInfo quantity or timestamp");
		return false;
	}
	if (Input.NowSeconds < Input.ChangeTimeSeconds)
	{
		OutError = TEXT("BuildingGainInfo changeTime is in the future");
		return false;
	}
	if (!Input.bCompleted)
	{
		return true;
	}

	const int64 Max = Input.GatherMax;
	const int64 Stored = FMath::Min(Input.StoredNum, Max);
	const int64 Elapsed = Input.NowSeconds - Input.ChangeTimeSeconds;
	const int64 Remaining = Max - Stored;
	// Remaining <= int32 max. This comparison caps huge time spans before multiplication.
	const int64 SecondsToCap = (Remaining * 3600 + Input.ProducePerHour - 1) / Input.ProducePerHour;
	if (Elapsed >= SecondsToCap)
	{
		OutResult.AccruedAmount = static_cast<double>(Max);
		OutResult.Accrued = Max;
	}
	else
	{
		const int64 ProducedNumerator = Elapsed * Input.ProducePerHour;
		OutResult.AccruedAmount = static_cast<double>(Stored) +
			static_cast<double>(ProducedNumerator) / 3600.0;
		OutResult.Accrued = Stored + ProducedNumerator / 3600;
	}
	// For integer gatherMin, exact unrounded >= gatherMin iff its floor is >= gatherMin.
	OutResult.bCanCollect = OutResult.AccruedAmount >= Input.GatherMin;
	OutResult.bAtCapacity = OutResult.Accrued >= Max;
	return true;
}

bool FRokCityResourceProduction::Collect(const FRokCityResourceProductionInput& Input,
	FRokCityResourceProductionResult& OutCollected,
	FRokCityResourceProductionInput& OutNextState, FString& OutError)
{
	OutCollected = {};
	OutNextState = Input;
	FRokCityResourceProductionResult Evaluated;
	if (!Evaluate(Input, Evaluated, OutError))
	{
		return false;
	}
	if (!Evaluated.bCanCollect)
	{
		OutError = TEXT("Production has not reached gatherMin or building is incomplete");
		return false;
	}
	OutCollected = Evaluated;
	OutNextState.StoredNum = 0;
	OutNextState.ChangeTimeSeconds = Input.NowSeconds;
	return true;
}
