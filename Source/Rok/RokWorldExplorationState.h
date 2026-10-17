#pragma once
#include "CoreMinimal.h"

// UI_Item_LodMenu_SubView.GetResIcon. Source IDs are positive MapFixPoint IDs.
// The Unity format uses ceil(id/64), NOT the usual floor(id/64) word index.
namespace RokWorldExplorationState
{
inline int32 WordIndex(int32 Id) { return Id > 0 ? static_cast<int32>((int64(Id) + 63) / 64) : 0; }
inline bool IsOpened(int32 Id, const TMap<int32, uint64>& Words)
{
	if (Id <= 0) return false;
	const uint64* Word = Words.Find(WordIndex(Id));
	return Word && ((*Word & (uint64(1) << (Id % 64))) != 0);
}
}
