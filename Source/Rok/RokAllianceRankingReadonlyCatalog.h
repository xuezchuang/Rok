#pragma once
#include "RokLeaderboardCatalog.h"

namespace RokAllianceRankingReadonlyCatalog
{
	ROK_API bool Load(TArray<FRokLeaderboardRow>& Rows, FString& Error);
}