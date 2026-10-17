#pragma once
#include "CoreMinimal.h"

// UI_Win_PlayerHeadPic + UI_LC_PlayerHeadPic. Chinese Awake flips the
// two ArabLayout PosX panels; ListView replaces prefab's saved -20 row offset.
namespace RokPlayerHeadLayout
{
inline FVector2D GridBackgroundPosition() { return FVector2D(543.6, 177.55); }
inline FVector2D GridBackgroundSize() { return FVector2D(652, 506.9); }
inline FVector2D ViewportPosition() { return FVector2D(557, 191.15); }
inline FVector2D ViewportSize() { return FVector2D(623.6, 477.3); }
inline FVector2D CellPosition(int32 Index) { return FVector2D(26.8 + (Index % 4) * 145, (Index / 4) * 135); }
inline double ContentHeight(int32 Count) { return FMath::DivideAndRoundUp(Count, 4) * 135.0; }
inline FVector2D PreviewPosition() { return FVector2D(274.15, 186); }
inline FVector2D TabPosition(int32 Index) { return FVector2D(1259, 168 + Index * 113); }
inline FVector2D ConfirmPosition() { return FVector2D(270.8, 597); }
}
