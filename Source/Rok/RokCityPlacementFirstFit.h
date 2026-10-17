#pragma once

#include "CoreMinimal.h"

/** Unity CityGlobalMediator.GetRandBuildLogic perimeter order, without mutation. */
struct ROK_API FRokCityPlacementFirstFit
{
	static bool Find(int32 SourceGridSize, const FIntPoint& FootprintSize,
		TFunctionRef<bool(const FIntPoint&)> IsBuildable, FIntPoint& OutOrigin);
};
