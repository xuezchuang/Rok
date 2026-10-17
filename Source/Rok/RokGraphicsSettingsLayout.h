#pragma once
#include "CoreMinimal.h"

// UI_Item_GraphicsSetting + Chinese ArabLayout flags32: RightToLeft becomes LeftToRight.
namespace RokGraphicsSettingsLayout
{
inline int32 Tier(float Value) { return FMath::Clamp(FMath::RoundToInt(Value), 1, 3); }
inline int32 RecommendedTier(uint64 MemoryMB) { return MemoryMB > 3000 ? 3 : MemoryMB > 2000 ? 2 : 1; }
inline FVector2D CardSize() { return FVector2D(1000, 367); }
inline FVector2D SliderPosition(int32 Row) { return FVector2D(56, 89.5 + Row * 182); }
// Slate subtracts one thumb width with IndentHandle=false; resulting center travel is 67..933.
inline FVector2D SliderSize() { return FVector2D(888, 41); }
inline FVector2D FillPosition(int32 Row) { return FVector2D(90, 101.3 + Row * 182); }
inline FVector2D FillSize(int32 Level) { return FVector2D(820 * (Tier(Level) - 1) / 2.0, 17.4); }
}
