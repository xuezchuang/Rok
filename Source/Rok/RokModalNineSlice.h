#pragma once

#include "CoreMinimal.h"
#include "Components/Border.h"

// Unity Image.GenerateSlicedSprite / GetAdjustedBorders: borders are sprite pixels,
// not fractions of the atlas resource used by Slate's Box element batcher.
namespace RokModalNineSlice
{
struct FPatch
{
	FIntRect Source;
	FVector2D Position;
	FVector2D Size;
	// Unity still emits a stretched quad when inner UVs coincide (for example
	// ActionBackground has 10 + 10 borders in a 20-pixel sprite).
	bool IsVisible() const { return Source.Width() >= 0 && Source.Height() >= 0 && Size.X > 0 && Size.Y > 0; }
};

inline FPatch Patch(const FIntRect& Rect, const FMargin& Margin, const FVector2D& Size, int32 Index)
{
	const int32 W = FMath::Max(0, Rect.Width()), H = FMath::Max(0, Rect.Height());
	if (W == 0 || H == 0) return {Rect, FVector2D::ZeroVector, FVector2D::ZeroVector};
	const int32 L = FMath::Clamp(FMath::RoundToInt(Margin.Left * W), 0, W);
	const int32 R = FMath::Clamp(FMath::RoundToInt(Margin.Right * W), 0, W - L);
	const int32 T = FMath::Clamp(FMath::RoundToInt(Margin.Top * H), 0, H);
	const int32 B = FMath::Clamp(FMath::RoundToInt(Margin.Bottom * H), 0, H - T);
	const double Width = FMath::Max(0.0, Size.X), Height = FMath::Max(0.0, Size.Y);
	const double SX = L + R > 0 ? FMath::Min(1.0, Width / (L + R)) : 1.0;
	const double SY = T + B > 0 ? FMath::Min(1.0, Height / (T + B)) : 1.0;
	const int32 X[] = {Rect.Min.X, Rect.Min.X + L, Rect.Max.X - R};
	// Unity atlas rects are bottom-up; UMG destination rows are top-down.
	const int32 Y[] = {Rect.Max.Y - T, Rect.Min.Y + B, Rect.Min.Y};
	const int32 SW[] = {L, W - L - R, R}, SH[] = {T, H - T - B, B};
	const double DX[] = {0, L * SX, Width - R * SX}, DY[] = {0, T * SY, Height - B * SY};
	const double DW[] = {L * SX, FMath::Max(0.0, Width - (L + R) * SX), R * SX};
	const double DH[] = {T * SY, FMath::Max(0.0, Height - (T + B) * SY), B * SY};
	const int32 Column = FMath::Clamp(Index, 0, 8) % 3, Row = FMath::Clamp(Index, 0, 8) / 3;
	return {FIntRect(X[Column], Y[Row], X[Column] + SW[Column], Y[Row] + SH[Row]),
		FVector2D(DX[Column], DY[Row]), FVector2D(DW[Column], DH[Row])};
}

inline void SetTint(UBorder* Border, const FLinearColor& Color)
{
	// Keep the public brush-color state for existing readers, and tint the actual
	// image children through SBorder's inherited widget style exactly once.
	Border->SetBrushColor(Color);
	Border->SetContentColorAndOpacity(Color);
}
}
