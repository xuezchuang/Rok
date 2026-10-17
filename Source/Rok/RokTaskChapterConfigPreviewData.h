#pragma once

#include "CoreMinimal.h"

// Source configuration only. Player chapter, unlock, progress and reward state are unknown.
struct FRokTaskChapterLocaleText
{
	int32 Id = 0;
	FString ZhCn;
};

struct FRokTaskChapterPreviewRow
{
	int32 Id = 0;
	bool bMetadataKnown = false;
	int32 SourceCsvLine = 0; // Zero only when TaskChapterData has no row.
	FRokTaskChapterLocaleText Title;
	FRokTaskChapterLocaleText Description1;
	FRokTaskChapterLocaleText Description2;
	FRokTaskChapterLocaleText PreviewText;
	FString PreviewImage;
	FString PreviewBackground;
	FString PreviewSound;
	TArray<int32> DialogBeginIds;
	TArray<int32> DialogEndIds;
	int32 RewardGroupId = 0; // Config reference only; no reward contents or claim action.
	TArray<FString> CivilizationCharacterAssets;
	TArray<FString> AgeCharacterAssets;
	TArray<int32> TaskIds;

	FString DisplayTitle() const;
};

struct FRokTaskChapterTaskPreviewRow
{
	int32 Id = 0;
	int32 ChapterId = 0;
	int32 Order = 0;
	int32 SourceCsvLine = 0;
	FRokTaskChapterLocaleText Title;
	FRokTaskChapterLocaleText Description;
	int32 Type = 0;
	int32 Param1 = 0;
	int32 Param2 = 0;
	int32 Require = 0;
	int32 RewardGroupId = 0; // Config reference only.
	FString IconSourceKey;
};

namespace RokTaskChapterConfigPreviewData
{
	ROK_API bool LoadAll(TArray<FRokTaskChapterPreviewRow>& OutChapters,
		TArray<FRokTaskChapterTaskPreviewRow>& OutTasks, FString& Error);
	ROK_API bool FindChapterById(const TArray<FRokTaskChapterPreviewRow>& Chapters,
		int32 Id, FRokTaskChapterPreviewRow& OutChapter);
	ROK_API bool FindTaskById(const TArray<FRokTaskChapterTaskPreviewRow>& Tasks,
		int32 Id, FRokTaskChapterTaskPreviewRow& OutTask);
#if WITH_DEV_AUTOMATION_TESTS
	ROK_API bool LoadBytesForTesting(TConstArrayView<uint8> Bytes,
		TArray<FRokTaskChapterPreviewRow>& OutChapters,
		TArray<FRokTaskChapterTaskPreviewRow>& OutTasks, FString& Error);
#endif
}
