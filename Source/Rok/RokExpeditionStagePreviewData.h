#pragma once

#include "CoreMinimal.h"

/** Source configuration only. No unlock, star, battle, reward-claim or account state. */
struct FRokExpeditionStagePreviewRow
{
	int32 Id = 0;
	int32 Level = 0;
	int32 FrontNumber = 0;
	FIntPoint BattleCoordinate = FIntPoint::ZeroValue;
	int32 Type = 0;
	int32 BattleId = 0;
	int32 TimeLimitSeconds = 0;
	int32 TroopsNumber = 0;
	TArray<int32> StarDescriptionIds;
	TArray<FString> StarDescriptions;
	int32 FirstRewardGroup = 0;
	TArray<int32> RewardGroups;
};

namespace RokExpeditionStagePreviewData
{
	ROK_API bool LoadAll(TArray<FRokExpeditionStagePreviewRow>& OutRows, FString& Error);
	ROK_API bool FindById(const TArray<FRokExpeditionStagePreviewRow>& Rows, int32 Id,
		FRokExpeditionStagePreviewRow& OutRow);
#if WITH_DEV_AUTOMATION_TESTS
	ROK_API bool LoadBytesForTesting(TConstArrayView<uint8> Bytes,
		TArray<FRokExpeditionStagePreviewRow>& OutRows, FString& Error);
#endif
}
