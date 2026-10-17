#pragma once

#include "CoreMinimal.h"

// ItemPackageShow + Currency.csv preview metadata. This is not an account reward or claim payload.
struct FRokTaskRewardPreviewRow
{
	int32 PackageRowId = 0;
	int32 CurrencyId = 0;
	int64 Quantity = 0;
	int32 LocaleId = 0;
	FString DisplayName;
	FString IconKey;
	FIntRect IconRect;
	FIntPoint IconAtlasSize = FIntPoint::ZeroValue;
};

namespace RokTaskRewardPreviewData
{
	ROK_API bool LoadGroup(int32 GroupId, TArray<FRokTaskRewardPreviewRow>& OutRows, FString& Error);
#if WITH_DEV_AUTOMATION_TESTS
	// Exercises the same byte-identity/parser path without mutating the generated catalog on disk.
	ROK_API bool LoadGroupBytesForTesting(int32 GroupId, TConstArrayView<uint8> Bytes,
		TArray<FRokTaskRewardPreviewRow>& OutRows, FString& Error);
#endif
}
