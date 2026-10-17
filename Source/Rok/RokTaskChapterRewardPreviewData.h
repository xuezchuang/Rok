#pragma once

#include "CoreMinimal.h"

// Configuration-only ItemPackageShow preview. No account ownership or claim state.
struct FRokTaskChapterRewardPreviewRow
{
	int32 PackageRowId = 0;
	int32 RewardGroupId = 0;
	int32 RewardType = 0;
	int32 TypeData = 0;
	int64 Quantity = 0;
	int32 LocaleId = 0;
	FString DisplayNameZhCn;
	FString IconKey;
	FString AtlasObjectPath;
	FIntRect IconRect;
	FIntPoint IconAtlasSize = FIntPoint::ZeroValue;
	int32 StableOrder = 0;
};

struct FRokTaskChapterRewardGroup
{
	int32 RewardGroupId = 0;
	int32 TaskId = 0; // zero only for a chapter aggregate
	int32 ChapterId = 0;
	bool bChapterAggregate = false;
	bool bMetadataKnown = false;
	TArray<FRokTaskChapterRewardPreviewRow> Rows;
};

namespace RokTaskChapterRewardPreviewData
{
	ROK_API bool LoadAll(TArray<FRokTaskChapterRewardGroup>& OutGroups, FString& Error);
	ROK_API bool FindForTask(const TArray<FRokTaskChapterRewardGroup>& Groups, int32 TaskId,
		FRokTaskChapterRewardGroup& Out);
	ROK_API bool FindForChapter(const TArray<FRokTaskChapterRewardGroup>& Groups, int32 ChapterId,
		FRokTaskChapterRewardGroup& Out);
#if WITH_DEV_AUTOMATION_TESTS
	ROK_API bool LoadBytesForTesting(TConstArrayView<uint8> Bytes,
		TArray<FRokTaskChapterRewardGroup>& OutGroups, FString& Error);
#endif
}
