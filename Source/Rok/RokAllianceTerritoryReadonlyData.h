#pragma once

#include "CoreMinimal.h"

/** Source-pinned static definitions; no server or account territory state. */
namespace RokAllianceTerritoryReadonly
{
struct FGroup
{
    int32 Index;
    int32 TitleLocalizationId;
    int32 HeaderLocalizationId;
};

struct FRow
{
    int32 UnityType;
    int32 GroupIndex;
    int32 NameLocalizationId;
    int32 DescriptionLocalizationId;
    int32 DetailLocalizationId;
    const TCHAR* SourceIconReference;
    bool bSourceIconMissing;
};

const TArray<FGroup>& GetGroups();
const TArray<FRow>& GetRows();
FString GetChineseSourceText(int32 LocalizationId);
const FRow* FindRow(int32 UnityType);
}
