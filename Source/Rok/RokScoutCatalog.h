#pragma once
#include "CoreMinimal.h"

struct FRokScoutCampLevel
{
	int32 Level = 0;
	int32 Capacity = 0;
	int32 View = 0;
	int32 SpeedMulti = 0;
};

struct ROK_API FRokScoutCatalog
{
	TArray<FRokScoutCampLevel> Levels;
	TMap<int32, FString> Text;
	bool Load(FString& Error);
	// Unknown, absent, and out-of-table levels do not fall back to level one.
	const FRokScoutCampLevel* FindLevel(int32 Level) const;
};
