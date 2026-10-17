#pragma once
#include "CoreMinimal.h"

// UIPlayerData/UI_Win_ChangeName.prefab, 1440x810 design canvas.
// Text/Placeholder: width -40, 24 px, MiddleRight + ArabLayout8 -> Chinese left.
// lbl_des: 20 px, UpperRight + ArabLayout10 -> Chinese upper-left.
namespace RokChangeNameLayout
{
inline FVector2D InputPosition() { return FVector2D(329, 270); }
inline FVector2D InputSize() { return FVector2D(782, 54); }
inline FVector2D DescriptionPosition() { return FVector2D(329, 371.9); }
inline FVector2D DescriptionSize() { return FVector2D(782, 103.6); }
constexpr int32 InputFontPoints = 18;
constexpr int32 DescriptionFontPoints = 15;
constexpr float InputHorizontalPadding = 20;
}
