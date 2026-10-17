#pragma once
#include "CoreMinimal.h"

struct FRokLeaderboardRow
{
	int32 Id = 0, Target = 0, Type = 0, ShowLimit = 0;
	FString Name, Icon;
	TArray<FString> Columns;
	FIntRect IconRect;
	bool HasIcon() const { return !Icon.IsEmpty(); }
};

namespace RokLeaderboardCatalog
{
	ROK_API bool Load(TArray<FRokLeaderboardRow>& Rows, FString& Error);
}
