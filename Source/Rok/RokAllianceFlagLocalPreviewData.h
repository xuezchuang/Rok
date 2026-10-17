#pragma once

#include "CoreMinimal.h"

namespace RokAllianceFlagLocalPreview
{
struct FEntry
{
    int32 Id;
    int32 Type;
    FIntRect ShowRect;
    FIntRect RealityRect;
    FColor Rgb;
    bool bHasSprite;
    bool bHasColor;
};

struct FDraft
{
    int32 ClothId = 102;
    int32 ClothColorId = 207;
    int32 EmblemId = 324;
    int32 EmblemColorId = 406;

    int32 GetId(int32 Type) const
    {
        switch (Type)
        {
        case 1: return ClothId;
        case 2: return ClothColorId;
        case 3: return EmblemId;
        case 4: return EmblemColorId;
        default: return INDEX_NONE;
        }
    }
    void SetId(int32 Type, int32 Id)
    {
        switch (Type)
        {
        case 1: ClothId = Id; break;
        case 2: ClothColorId = Id; break;
        case 3: EmblemId = Id; break;
        case 4: EmblemColorId = Id; break;
        default: break;
        }
    }
};

const TArray<FEntry>& Rows();
const FEntry* Find(int32 Type, int32 Id);
bool IsValidDraft(const FDraft& Draft);
FLinearColor LinearColor(const FEntry& Entry);
}
