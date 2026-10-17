#pragma once
#include "CoreMinimal.h"

namespace RokTrainParameterLayout
{
// Unity 1440x810 canvas, after Chinese ArabLayoutCompment.Awake.
inline FVector2D RightOrigin() { return FVector2D(639.85, 98.4); }
inline FVector2D RightSize() { return FVector2D(624.1, 659); }
inline FVector2D HeadOrigin(int32 TierIndex) { return FVector2D(656.9 + 118 * TierIndex, 90.27); }
inline FVector2D HeadIcon(int32 TierIndex) { return HeadOrigin(TierIndex) + FVector2D(-4, 26); }
inline FVector2D HeadHit(int32 TierIndex) { return HeadOrigin(TierIndex) + FVector2D(6.3, 36.45); }
inline FVector2D InputPosition() { return FVector2D(1116.435, 429.2); }
inline FVector2D SliderPosition() { return FVector2D(684.5, 431.34); }
inline FVector2D CostPosition() { return FVector2D(677.9, 486.15); }
inline FVector2D TrainPosition() { return FVector2D(972.9, 610.4); }
}
