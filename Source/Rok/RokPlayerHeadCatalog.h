#pragma once
#include "CoreMinimal.h"

struct FRokPlayerHeadRow
{
	int32 Id = 0;
	int32 Group = 0;
	bool bFree = false;
	bool bArtAvailable = false;
	FString Name, Description, Tag, Icon, Texture;
	FIntRect Rect;
	FIntPoint AtlasSize = FIntPoint::ZeroValue;
};

namespace RokPlayerHeadCatalog
{
	// Reads generated config only; never treats browsing a row as ownership.
	ROK_API bool Load(TArray<FRokPlayerHeadRow>& Rows, FString& Error);
}
